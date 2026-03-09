#include "AntColonySystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include "TopologicalSort.h"
#include <iostream>
#include <random>
#include <chrono>
#include <cstddef>

namespace {
    // Всегда получаем новый seed для каждого запуска алгоритма.
    static unsigned runtime_seed(unsigned salt = 0u)
    {
        std::random_device rd;
        const unsigned t = (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return rd() ^ (t + 0x9e3779b9u + (salt << 6) + (salt >> 2));
    }
}

namespace scheduling_problem::algorithms {

ScheduleStatus AntColonySystem::ArtificialAnt::makeRoute(
    const Graph &graph,
    Matrix &matrix,
    double phe_influence,
    double heu_influence,
    double threshold,
    randgen &rng)
{
auto num_vertices = boost::num_vertices(graph);
ScheduleStatus status(graph);

// Строим маршрут в ТОПОЛОГИЧЕСКОМ порядке, чтобы все предки были вставлены раньше потомка.
auto topo = scheduling_problem::topo_sort(graph);
if (topo.empty()) return status;

status.insert(static_cast<size_t>(topo[0]), 0, graph);

for (size_t step = 1; step < topo.size(); ++step)
{
    const size_t curr_vid = static_cast<size_t>(topo[step]);
    auto pos = choice(graph, matrix, status, curr_vid,
                      phe_influence, heu_influence,
                      threshold, rng);
    status.insert(curr_vid, pos, graph);
}
return status;

}

size_t AntColonySystem::ArtificialAnt::choice(
    const Graph &graph,
    Matrix &matrix,
    const ScheduleStatus &status,
    size_t curr_vid,
    double phe_influence,
    double heu_influence,
    double threshold,
    randgen &rng)
{
    auto desirability = heuInfo(graph, status, curr_vid);
    const size_t lower = status.lower(curr_vid, graph);
    const size_t upper = status.upper(curr_vid, graph); // exclusive

    for (auto it = desirability.begin(); it != desirability.end(); ) {
        if (it->first < lower || it->first >= upper) it = desirability.erase(it);
        else ++it;
    }
    if (desirability.empty())
        return lower;

    constexpr double kEps = 1e-12;
    for (auto &[pos, heu_val] : desirability)
    {
        double phe_val = 1.0;
        for (const auto &job : status)
        {
            const size_t prev_vid = job.id;
            if (status.loc(prev_vid) < pos)
                phe_val *= matrix[curr_vid][prev_vid];
            else
                phe_val *= matrix[prev_vid][curr_vid];
        }
        phe_val = std::max(phe_val, kEps);
        heu_val = std::max(heu_val, kEps);
        heu_val = std::pow(phe_val, phe_influence) * std::pow(heu_val, heu_influence);
    }

    std::uniform_real_distribution<double> uid(0, 1);
    double q = uid(rng);

    if (q > threshold)
    {
        double sum = 0.0;
        for (auto &[p, v] : desirability)
            sum += v;

        if (sum <= 0.0)
        {
            auto best = std::max_element(desirability.begin(), desirability.end(),
                                         [](const auto &a, const auto &b){ return a.second < b.second; });
            return best->first;
        }

        std::uniform_real_distribution<double> roul(0.0, sum);
        double pick = roul(rng);

        double acc = 0.0;
        for (auto &[p, v] : desirability)
        {
            acc += v;
            if (acc >= pick)
                return p;
        }
        auto best = std::max_element(
            desirability.begin(), desirability.end(),
            [](const auto& a, const auto& b){ return a.second < b.second; }
        );
        return best != desirability.end() ? best->first : 0;
    }

    auto best = std::max_element(desirability.begin(), desirability.end(),
                                 [](const auto &a, const auto &b){ return a.second < b.second; });
    return best->first;
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
    , epochs_count_(epochs_count)
    , ants_count_(ants_count)
    , best_count_(best_count)
    , rng_(seed)
    , rebuilder_(3000)
{
    ants_.assign(ants_count_, ArtificialAnt{});
}

Schedule AntColonySystem::schedule_(const Graph &graph, const Schedule &base_schedule)
{
    // RNG: каждый запуск ACO должен быть стохастическим (новый seed).
    rng_.seed(runtime_seed());

    // Проверка на наличие циклов в графе (входной граф должен быть DAG)
    const auto total_vertices = boost::num_vertices(graph);
    auto topo_order = scheduling_problem::topo_sort(graph);
    if (topo_order.size() != total_vertices) {
        std::cerr << "Error: input graph contains a cycle or is not a DAG. Terminating ACO.\n";
        return Schedule();
    }

    weight_t weight_max = 0;
    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
        auto w = boost::get(vertex_weight_t(), graph, vertex);
        if (w > weight_max) weight_max = w;
    }


    auto best_solution = base_schedule;

    if (!best_solution.size() || best_solution.cost() <= 0)
    {
        auto fallback = baseline_->schedule(graph);
        if (fallback.size() && fallback.cost() > 0)
            best_solution = fallback;
    }

    auto denom = best_solution.cost();
    if (denom <= 0) denom = 1;

    double init_trail = (double)weight_max / (double)denom;
    Matrix weight_matrix = makeMatrix(total_vertices, init_trail);

    unsigned stagnations = 0;
    iters_count_ = 0;

    // 2. Стартовый пул решений: Greedy + случайные
    ScheduleStatus status(graph, best_solution);
    std::vector<ScheduleStatus> solution_pool;
    solution_pool.reserve(best_count_ * 2);
    solution_pool.push_back(status);

    RandomSearch rsearch(3000);
    for (unsigned i = 1; i < best_count_ * 2; ++i)
    {
        auto rnd_sched = rsearch.generateRandomSchedule(graph);
        solution_pool.emplace_back(graph, rnd_sched);
    }

    std::sort(solution_pool.begin(), solution_pool.end(),
              [](const auto &a, const auto &b){ return a.cost() < b.cost(); });

    if (solution_pool.size() > best_count_)
        solution_pool.erase(solution_pool.begin() + best_count_, solution_pool.end());

    best_solution = solution_pool.front();

    // 3. Основной цикл по эпохам
    for (unsigned epoch = 0; epoch < epochs_count_; ++epoch)
    {
        globalPheUpdate(solution_pool, weight_matrix, weight_max);

        auto epoch_routes = completeEpoch(graph, weight_matrix, init_trail);

        if (!epoch_routes.empty() && epoch_routes.front().cost() < best_solution.cost())
        {
            auto improvement_percentage =
                ((double)best_solution.cost() - epoch_routes.front().cost()) / (double)best_solution.cost();

            if (saturation_ && improvement_percentage > improvement_)
                stagnations = 0;

            best_solution = epoch_routes.front();
        }
        else
        {
            stagnations++;
        }

        solution_pool.insert(solution_pool.end(), epoch_routes.begin(), epoch_routes.end());
        std::sort(solution_pool.begin(), solution_pool.end(),
                  [](const auto &a, const auto &b){ return a.cost() < b.cost(); });

        if (solution_pool.size() > best_count_)
            solution_pool.erase(solution_pool.begin() + best_count_, solution_pool.end());

        iters_count_++;
        cost_dynamics_.push_back(best_solution.cost());

        if (saturation_ && stagnations >= saturation_)
            break;
    }

    return best_solution;
}

std::vector<ScheduleStatus> AntColonySystem::completeEpoch(
    const Graph &graph,
    Matrix &matrix,
    double init_trail)
{
    std::vector<ScheduleStatus> epoch_routes;
    epoch_routes.reserve(ants_count_);

    for (unsigned i = 0; i < ants_count_; ++i)
    {
        auto status = ants_[i].makeRoute(graph, matrix, phe_influence_, heu_influence_, threshold_, rng_);

        double delta = init_trail;
        if (status.cost() > 0)
            delta = (double)init_trail * ((double)status.cost());

        localPheUpdate(status, matrix, delta);
        epoch_routes.push_back(std::move(status));
    }

    std::sort(epoch_routes.begin(), epoch_routes.end(),
              [](const auto &a, const auto &b){ return a.cost() < b.cost(); });

    return epoch_routes;
}

AntColonySystem::Matrix AntColonySystem::makeMatrix(size_t dim, double val)
{
    return Matrix(dim, std::vector<double>(dim, val));
}

void AntColonySystem::localPheUpdate(const ScheduleStatus &status,
                                     Matrix &matrix,
                                     double delta) const
{
    auto reinforcement = phe_decay_ * delta;
    auto alpha = 1 - phe_decay_;

    for (size_t curr_vid = 1; curr_vid < status.size(); ++curr_vid)
    {
        for (size_t prev_vid = 0; prev_vid < curr_vid; ++prev_vid)
        {
            if (status.loc(prev_vid) < status.loc(curr_vid))
                matrix[prev_vid][curr_vid] = alpha * matrix[prev_vid][curr_vid] + reinforcement;
            else
                matrix[curr_vid][prev_vid] = alpha * matrix[curr_vid][prev_vid] + reinforcement;
        }
    }
}

void AntColonySystem::globalPheUpdate(const std::vector<ScheduleStatus> &epoch_routes,
                                     Matrix &matrix,
                                     double weight_max) const
{
    for (auto &row : matrix)
        for (auto &elem : row)
            elem = (1 - evaporation_) * elem;

    for (auto &route : epoch_routes)
    {
        double denom = (double)route.cost();
        if (denom <= 0) denom = 1.0;
        auto reinforcement = evaporation_ * (double)weight_max / denom;

        for (size_t u = 0; u < route.size() - 1; ++u)
        {
            for (size_t v = u + 1; v < route.size(); ++v)
            {
                if (route.loc(u) < route.loc(v))
                    matrix[u][v] += reinforcement;
                else
                    matrix[v][u] += reinforcement;
            }
        }
    }
}
std::unique_ptr<BaseOptimization> AntColonySystem::copy() const
{
    return std::unique_ptr<BaseOptimization>(
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
}

}
