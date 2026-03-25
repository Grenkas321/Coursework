#include "AntColonySystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>

#include "TopologicalSort.h"

namespace {
    static unsigned runtime_seed(unsigned salt = 0u)
    {
        std::random_device rd;
        const unsigned t = (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return rd() ^ (t + 0x9e3779b9u + (salt << 6) + (salt >> 2));
    }

    using Route = scheduling_problem::algorithms::AntColonySystem::Route;
    using Matrix = scheduling_problem::algorithms::AntColonySystem::Matrix;

    constexpr double kEps = 1e-12;

    scheduling_problem::weight_t durationOf(const scheduling_problem::Graph &graph, size_t v)
    {
        auto exec = boost::get(scheduling_problem::vertex_exec_time_t(), graph);
        auto w = boost::get(scheduling_problem::vertex_weight_t(), graph);
        const auto d = exec[v];
        return d > 0 ? d : std::max<scheduling_problem::weight_t>(1, w[v]);
    }

    scheduling_problem::weight_t totalDuration(const scheduling_problem::Graph &graph)
    {
        scheduling_problem::weight_t total = 0;
        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
            total += durationOf(graph, v);
        return std::max<scheduling_problem::weight_t>(1, total);
    }

    std::vector<scheduling_problem::weight_t> bottomLevels(const scheduling_problem::Graph &graph)
    {
        const size_t n = boost::num_vertices(graph);
        auto topo = scheduling_problem::topo_sort(graph);
        if (topo.size() != n)
            throw std::runtime_error("Cannot continue ACO scheduling: graph is not a DAG.");

        std::vector<scheduling_problem::weight_t> bottom(n, 0);
        for (auto it = topo.rbegin(); it != topo.rend(); ++it)
        {
            const auto v = static_cast<size_t>(*it);
            scheduling_problem::weight_t best_child = 0;
            for (auto e : boost::make_iterator_range(boost::out_edges(v, graph)))
            {
                if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                    continue;
                best_child = std::max(best_child, bottom[boost::target(e, graph)]);
            }
            bottom[v] = durationOf(graph, v) + best_child;
        }
        return bottom;
    }

    bool betterRoute(const Route &lhs, const Route &rhs)
    {
        if (lhs.eval.feasible() != rhs.eval.feasible())
            return lhs.eval.feasible() && !rhs.eval.feasible();
        if (std::fabs(lhs.score - rhs.score) > kEps)
            return lhs.score < rhs.score;
        if (lhs.eval.makespan != rhs.eval.makespan)
            return lhs.eval.makespan < rhs.eval.makespan;
        if (lhs.eval.peak_memory != rhs.eval.peak_memory)
            return lhs.eval.peak_memory < rhs.eval.peak_memory;
        return lhs.proc_choice < rhs.proc_choice;
    }

    Route routeFromSchedule(const scheduling_problem::Graph &graph,
                            const scheduling_problem::Schedule &schedule,
                            unsigned processors,
                            scheduling_problem::weight_t memory_limit,
                            double overflow_penalty)
    {
        Route route;
        const size_t n = boost::num_vertices(graph);
        route.state = schedule.empty()
            ? scheduling_problem::algorithms::makeListState(graph, processors)
            : scheduling_problem::algorithms::makeStateFromSchedule(graph, schedule, processors);
        route.eval = scheduling_problem::algorithms::evaluateLayeredState(graph, route.state, memory_limit);
        route.score = route.eval.score(overflow_penalty);
        route.proc_choice.assign(n, 0);
        route.prev_on_proc.assign(n, n);

        for (unsigned p = 0; p < route.state.processors.size(); ++p)
        {
            size_t prev = n;
            for (const auto task : route.state.processors[p])
            {
                route.proc_choice[task] = p;
                route.prev_on_proc[task] = prev;
                prev = task;
            }
        }

        return route;
    }
}

namespace scheduling_problem::algorithms {

AntColonySystem::Route AntColonySystem::ArtificialAnt::makeRoute(
    const Graph &graph,
    const std::vector<weight_t> &bottom_levels,
    unsigned processors,
    weight_t memory_limit,
    double overflow_penalty,
    const Matrix &proc_matrix,
    const Matrix &order_matrix,
    double phe_influence,
    double heu_influence,
    double threshold,
    randgen &rng) const
{
    struct Action
    {
        size_t task = 0;
        unsigned processor = 0;
        size_t prev_on_proc = 0;
        weight_t start = 0;
        weight_t finish = 0;
        double desirability = 0.0;
    };

    const size_t n = boost::num_vertices(graph);
    const unsigned pcount = std::max(1u, processors);
    const weight_t max_bottom = bottom_levels.empty()
        ? 1
        : std::max<weight_t>(1, *std::max_element(bottom_levels.begin(), bottom_levels.end()));

    Route route;
    route.state.processors.assign(pcount, {});
    route.state.proc_of.assign(n, 0);
    route.state.tier_of.assign(n, 0);
    route.proc_choice.assign(n, 0);
    route.prev_on_proc.assign(n, n);

    std::vector<std::vector<size_t>> children(n);
    std::vector<unsigned> indeg(n, 0);
    for (auto e : boost::make_iterator_range(boost::edges(graph)))
    {
        if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real)
            continue;
        const auto u = static_cast<size_t>(boost::source(e, graph));
        const auto v = static_cast<size_t>(boost::target(e, graph));
        children[u].push_back(v);
        indeg[v]++;
    }

    std::vector<size_t> ready;
    ready.reserve(n);
    for (size_t v = 0; v < n; ++v)
        if (indeg[v] == 0)
            ready.push_back(v);
    std::sort(ready.begin(), ready.end());

    std::vector<weight_t> proc_free(pcount, 0);
    std::vector<weight_t> pred_ready(n, 0);
    std::vector<weight_t> finish_est(n, 0);
    std::vector<size_t> last_task(pcount, n);
    auto vertex_weight = boost::get(vertex_weight_t(), graph);

    while (!ready.empty())
    {
        std::vector<Action> actions;
        actions.reserve(ready.size() * pcount);

        for (const auto task : ready)
        {
            const auto dur = durationOf(graph, task);
            const double criticality =
                1.0 + static_cast<double>(bottom_levels[task]) / static_cast<double>(max_bottom);
            double memory_bias = 1.0;
            if (memory_limit != std::numeric_limits<weight_t>::max() && memory_limit > 0)
            {
                memory_bias =
                    1.0 / (1.0 + static_cast<double>(std::max<weight_t>(0, vertex_weight[task])) /
                                     static_cast<double>(memory_limit));
            }

            for (unsigned p = 0; p < pcount; ++p)
            {
                const auto start = std::max(pred_ready[task], proc_free[p]);
                const auto finish = start + dur;
                const double eta =
                    std::max(kEps,
                             criticality * memory_bias /
                                 (1.0 + static_cast<double>(finish)) *
                                 (1.0 + 1.0 / (1.0 + static_cast<double>(route.state.processors[p].size()))));

                const size_t prev = last_task[p];
                const double tau =
                    std::max(kEps, proc_matrix[task][p]) *
                    std::max(kEps, order_matrix[prev][task]);
                const double desirability =
                    std::pow(tau, phe_influence) * std::pow(eta, heu_influence);

                actions.push_back({task, p, prev, start, finish, desirability});
            }
        }

        auto best_it = std::max_element(
            actions.begin(), actions.end(),
            [](const Action &lhs, const Action &rhs)
            {
                if (std::fabs(lhs.desirability - rhs.desirability) > kEps)
                    return lhs.desirability < rhs.desirability;
                if (lhs.finish != rhs.finish)
                    return lhs.finish > rhs.finish;
                if (lhs.start != rhs.start)
                    return lhs.start > rhs.start;
                if (lhs.task != rhs.task)
                    return lhs.task > rhs.task;
                return lhs.processor > rhs.processor;
            });

        Action chosen = *best_it;
        std::uniform_real_distribution<double> uid(0.0, 1.0);
        if (uid(rng) > threshold)
        {
            double sum = 0.0;
            for (const auto &action : actions)
                sum += action.desirability;
            if (sum > kEps)
            {
                std::uniform_real_distribution<double> roulette(0.0, sum);
                const double pick = roulette(rng);
                double acc = 0.0;
                for (const auto &action : actions)
                {
                    acc += action.desirability;
                    if (acc >= pick)
                    {
                        chosen = action;
                        break;
                    }
                }
            }
        }

        const auto task = chosen.task;
        const auto p = chosen.processor;
        route.proc_choice[task] = p;
        route.prev_on_proc[task] = chosen.prev_on_proc;
        route.state.proc_of[task] = p;
        route.state.tier_of[task] = route.state.processors[p].size();
        route.state.processors[p].push_back(task);

        last_task[p] = task;
        proc_free[p] = chosen.finish;
        finish_est[task] = chosen.finish;

        ready.erase(std::remove(ready.begin(), ready.end(), task), ready.end());
        for (const auto child : children[task])
        {
            pred_ready[child] = std::max(pred_ready[child], finish_est[task]);
            if (--indeg[child] == 0)
                ready.push_back(child);
        }
        std::sort(ready.begin(), ready.end());
    }

    route.eval = evaluateLayeredState(graph, route.state, memory_limit);
    route.score = route.eval.score(overflow_penalty);
    return route;
}

AntColonySystem::AntColonySystem(BaseOptimization &baseline,
                                 double evaporation,
                                 double phe_decay,
                                 double phe_influence,
                                 double heu_influence,
                                 double threshold,
                                 unsigned epochs_count,
                                 unsigned ants_count,
                                 unsigned best_count,
                                 unsigned saturation,
                                 double improvement,
                                 unsigned seed,
                                 const std::string &label)
    : IterativeOptimization(baseline, saturation, improvement, seed, label)
    , evaporation_(evaporation)
    , phe_decay_(phe_decay)
    , phe_influence_(phe_influence)
    , heu_influence_(heu_influence)
    , threshold_(threshold)
    , epochs_count_(std::max(1u, epochs_count))
    , ants_count_(std::max(1u, ants_count))
    , best_count_(std::max(1u, best_count))
    , rng_(seed)
{
    ants_.assign(ants_count_, ArtificialAnt{});
}

Schedule AntColonySystem::schedule_(const Graph &graph, const Schedule &base_schedule)
{
    cost_dynamics_.clear();
    iters_count_ = 0;
    ants_.assign(std::max(1u, ants_count_), ArtificialAnt{});
    rng_.seed(runtime_seed(seed_));

    const size_t n = boost::num_vertices(graph);
    if (n == 0)
        return Schedule(0, graph.name());

    const auto bottom_levels = ::bottomLevels(graph);
    const double init_trail = 1.0;
    const double reward_scale =
        std::max<double>(1.0, static_cast<double>(totalDuration(graph)) / static_cast<double>(std::max(1u, processors_)));

    Matrix proc_matrix = makeMatrix(n, std::max(1u, processors_), init_trail);
    Matrix order_matrix = makeMatrix(n + 1, n, init_trail);

    Route best_route = routeFromSchedule(graph, base_schedule, processors_, memory_limit_, overflow_penalty_);
    bool have_feasible = best_route.eval.feasible();
    std::vector<Route> elite_routes{best_route};
    unsigned stagnations = 0;

    for (unsigned epoch = 0; epoch < epochs_count_; ++epoch)
    {
        globalPheUpdate(elite_routes, proc_matrix, order_matrix, reward_scale);

        auto epoch_routes = completeEpoch(graph,
                                          bottom_levels,
                                          proc_matrix,
                                          order_matrix,
                                          init_trail,
                                          reward_scale);

        if (!epoch_routes.empty() && betterRoute(epoch_routes.front(), best_route))
        {
            const auto previous_score = std::max(1.0, best_route.score);
            const auto improvement_percentage =
                (previous_score - epoch_routes.front().score) / previous_score;
            if (saturation_ == 0 || improvement_percentage > improvement_)
                stagnations = 0;
            else
                ++stagnations;

            best_route = epoch_routes.front();
            have_feasible = have_feasible || best_route.eval.feasible();
        }
        else
        {
            ++stagnations;
        }

        elite_routes.insert(elite_routes.end(), epoch_routes.begin(), epoch_routes.end());
        std::sort(elite_routes.begin(), elite_routes.end(), betterRoute);
        if (elite_routes.size() > best_count_)
            elite_routes.erase(elite_routes.begin() + best_count_, elite_routes.end());

        ++iters_count_;
        cost_dynamics_.push_back(best_route.eval.feasible()
                                     ? best_route.eval.makespan
                                     : std::numeric_limits<weight_t>::max() / 4);

        if (saturation_ && stagnations >= saturation_)
            break;
    }

    if (!have_feasible && memory_limit_ != std::numeric_limits<weight_t>::max())
    {
        throw std::runtime_error(
            "ACO could not find a feasible layered schedule under memory limit " +
            std::to_string(memory_limit_) + " for graph '" + graph.name() + "'.");
    }

    auto result = toSchedule(graph, best_route.state, best_route.eval);
    result.setCost(best_route.eval.makespan);
    return result;
}

std::vector<AntColonySystem::Route> AntColonySystem::completeEpoch(
    const Graph &graph,
    const std::vector<weight_t> &bottom_levels,
    Matrix &proc_matrix,
    Matrix &order_matrix,
    double init_trail,
    double reward_scale)
{
    std::vector<Route> epoch_routes;
    epoch_routes.reserve(ants_count_);

    for (unsigned i = 0; i < ants_count_; ++i)
    {
        auto route = ants_[i].makeRoute(graph,
                                        bottom_levels,
                                        processors_,
                                        memory_limit_,
                                        overflow_penalty_,
                                        proc_matrix,
                                        order_matrix,
                                        phe_influence_,
                                        heu_influence_,
                                        threshold_,
                                        rng_);

        double delta = init_trail;
        if (route.eval.feasible())
            delta += reward_scale / std::max(1.0, route.score);

        localPheUpdate(route, proc_matrix, order_matrix, delta);
        epoch_routes.push_back(std::move(route));
    }

    std::sort(epoch_routes.begin(), epoch_routes.end(), betterRoute);
    return epoch_routes;
}

AntColonySystem::Matrix AntColonySystem::makeMatrix(size_t rows, size_t cols, double val)
{
    return Matrix(rows, std::vector<double>(cols, val));
}

void AntColonySystem::localPheUpdate(const Route &route,
                                     Matrix &proc_matrix,
                                     Matrix &order_matrix,
                                     double delta) const
{
    const double reinforcement = phe_decay_ * delta;
    const double alpha = 1.0 - phe_decay_;

    for (size_t task = 0; task < route.proc_choice.size(); ++task)
    {
        const auto proc = route.proc_choice[task];
        const auto prev = route.prev_on_proc[task];
        proc_matrix[task][proc] = std::max(kEps, alpha * proc_matrix[task][proc] + reinforcement);
        order_matrix[prev][task] = std::max(kEps, alpha * order_matrix[prev][task] + reinforcement);
    }
}

void AntColonySystem::globalPheUpdate(const std::vector<Route> &epoch_routes,
                                      Matrix &proc_matrix,
                                      Matrix &order_matrix,
                                      double reward_scale) const
{
    for (auto &row : proc_matrix)
        for (auto &elem : row)
            elem = std::max(kEps, (1.0 - evaporation_) * elem);
    for (auto &row : order_matrix)
        for (auto &elem : row)
            elem = std::max(kEps, (1.0 - evaporation_) * elem);

    bool reinforced = false;
    for (const auto &route : epoch_routes)
    {
        if (!route.eval.feasible())
            continue;

        const double reinforcement = evaporation_ * reward_scale / std::max(1.0, route.score);
        for (size_t task = 0; task < route.proc_choice.size(); ++task)
        {
            proc_matrix[task][route.proc_choice[task]] += reinforcement;
            order_matrix[route.prev_on_proc[task]][task] += reinforcement;
        }
        reinforced = true;
    }

    if (!reinforced && !epoch_routes.empty())
    {
        const auto &route = epoch_routes.front();
        const double reinforcement = 0.1 * evaporation_ * reward_scale / std::max(1.0, route.score);
        for (size_t task = 0; task < route.proc_choice.size(); ++task)
        {
            proc_matrix[task][route.proc_choice[task]] += reinforcement;
            order_matrix[route.prev_on_proc[task]][task] += reinforcement;
        }
    }
}

void AntColonySystem::setParams(const ParamSet &params)
{
    for (const auto &[name, val] : params)
    {
        if (name == "evaporation")
            evaporation_ = std::clamp((double)val, 0.0, 1.0);
        else if (name == "phe_decay")
            phe_decay_ = std::clamp((double)val, 0.0, 1.0);
        else if (name == "phe_influence")
            phe_influence_ = std::max(0.0, (double)val);
        else if (name == "heu_influence")
            heu_influence_ = std::max(0.0, (double)val);
        else if (name == "threshold")
            threshold_ = std::clamp((double)val, 0.0, 1.0);
        else if (name == "epochs_count")
            epochs_count_ = std::max(1u, (unsigned)val);
        else if (name == "ants_count")
            ants_count_ = std::max(1u, (unsigned)val);
        else if (name == "best_count")
            best_count_ = std::max(1u, (unsigned)val);
        else if (name == "processors")
        {
            processors_ = std::max(1u, (unsigned)val);
            if (baseline_)
                baseline_->setParams({{"processors", processors_}});
        }
        else if (name == "memory_limit")
        {
            const double raw = (double)val;
            if (!std::isfinite(raw) || raw < 0)
                memory_limit_ = std::numeric_limits<weight_t>::max();
            else
                memory_limit_ = static_cast<weight_t>(raw);
            if (baseline_)
                baseline_->setParams({{"memory_limit", static_cast<double>(memory_limit_)}});
        }
        else if (name == "overflow_penalty")
            overflow_penalty_ = std::max(1.0, (double)val);
        else
            IterativeOptimization::setParams({{name, val}});
    }

    ants_.assign(ants_count_, ArtificialAnt{});
}

ParamSet AntColonySystem::getParams() const
{
    auto params = IterativeOptimization::getParams();
    params["evaporation"] = evaporation_;
    params["phe_decay"] = phe_decay_;
    params["phe_influence"] = phe_influence_;
    params["heu_influence"] = heu_influence_;
    params["threshold"] = threshold_;
    params["epochs_count"] = epochs_count_;
    params["ants_count"] = ants_count_;
    params["best_count"] = best_count_;
    params["processors"] = processors_;
    params["memory_limit"] = static_cast<double>(memory_limit_);
    params["overflow_penalty"] = overflow_penalty_;
    return params;
}

std::unique_ptr<BaseOptimization> AntColonySystem::copy() const
{
    auto cloned = std::unique_ptr<AntColonySystem>(
        new AntColonySystem(*baseline_,
                            evaporation_,
                            phe_decay_,
                            phe_influence_,
                            heu_influence_,
                            threshold_,
                            epochs_count_,
                            ants_count_,
                            best_count_,
                            saturation_,
                            improvement_,
                            seed_,
                            label_));
    cloned->setParams({
        {"processors", processors_},
        {"memory_limit", static_cast<double>(memory_limit_)},
        {"overflow_penalty", overflow_penalty_}
    });
    return cloned;
}

}
