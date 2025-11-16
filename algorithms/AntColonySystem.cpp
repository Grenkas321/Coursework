#include "AntColonySystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace scheduling_problem::algorithms {

ScheduleStatus AntColonySystem::ArtificialAnt::makeRoute(const Graph &graph,
                                                         Matrix &matrix,
                                                         double phe_influence,
                                                         double heu_influence,
                                                         double threshold,
                                                         randgen &rng)
{
    const auto n = boost::num_vertices(graph);
    ScheduleStatus status(graph);
    status.insert(0, 0, graph);
    for (size_t v = 1; v < n; ++v) {
        const size_t pos = choice(graph, matrix, status, v,
                                  phe_influence, heu_influence, threshold, rng);
        status.insert(v, pos, graph);
    }
    return status;
}

size_t AntColonySystem::ArtificialAnt::choice(const Graph &graph,
                                              const Matrix &matrix,
                                              const ScheduleStatus &status,
                                              size_t curr_vid,
                                              double phe_influence,
                                              double heu_influence,
                                              double threshold,
                                              randgen &rng)
{
    auto desir = heuInfo(graph, status, curr_vid); // по позициям

    for (auto& [pos, heu_val] : desir) {
        double tau = 1.0;
        for (size_t prev = 0; prev < curr_vid; ++prev) {
            if (status.loc(prev) < pos) tau *= matrix[prev][curr_vid];
            else                        tau *= matrix[curr_vid][prev];
        }
        heu_val = std::pow(std::max(1e-12, tau), phe_influence)
                * std::pow(std::max(1e-12, heu_val), heu_influence);
    }

    std::uniform_real_distribution<double> U(0.0, 1.0);
    if (U(rng) < threshold) {
        return std::max_element(desir.begin(), desir.end(),
                                [](const auto& a, const auto& b){
                                    return a.second < b.second;
                                })->first;
    }

    double sum = 0.0;
    for (const auto& kv : desir) sum += kv.second;
    if (!(sum > 0.0) || !std::isfinite(sum)) {
        auto it = desir.begin();
        return it->first; // fallback: первый
    }
    std::uniform_real_distribution<double> R(0.0, sum);
    double t = R(rng);
    for (const auto& kv : desir) {
        if ((t -= kv.second) <= 0.0) return kv.first;
    }
    return desir.begin()->first; // fallback
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
    weight_t wmax = 0;
    for (auto v : boost::make_iterator_range(boost::vertices(graph))) {
        wmax = std::max(wmax, boost::get(vertex_weight_t(), graph, v));
    }

    Schedule best = base_schedule;
    if (!best.size()) {
        Greedy g; best = g.schedule(graph);
    }
    weight_t denom = best.cost(graph);
    if (denom <= 0) denom = 1;

    const double init_trail = static_cast<double>(wmax) / static_cast<double>(denom);
    Matrix matrix = makeMatrix(boost::num_vertices(graph), init_trail);

    std::vector<ScheduleStatus> pool;
    pool.reserve(best_count_ * 2 + 1);
    pool.emplace_back(graph, best);

    RandomSearch rs(3000);
    for (unsigned i = 1; i < best_count_ * 2; ++i) {
        const auto rnd = rs.generateRandomSchedule(graph);
        pool.emplace_back(graph, rnd);
    }

    std::sort(pool.begin(), pool.end(),
              [](const auto& a, const auto& b){ return a.cost() < b.cost(); });
    if (pool.size() > best_count_) {
        pool.erase(pool.begin() + best_count_, pool.end());
    }

    ScheduleStatus best_status = pool.front();
    best = best_status;

    unsigned stagn = 0;
    iters_count_ = 0;
    ants_.assign(ants_count_, ArtificialAnt{});

    for (unsigned epoch = 0; epoch < epochs_count_; ++epoch) {
        globalPheUpdate(pool, matrix, wmax);

        auto routes = completeEpoch(graph, matrix, init_trail);

        if (!routes.empty() && routes.front().cost() < best_status.cost()) {
            const double rel_imp = (double)(best_status.cost() - routes.front().cost())
                                   / std::max<weight_t>(1, best_status.cost());
            if (rel_imp > improvement_) stagn = 0;
            best_status = routes.front();
            best = best_status;
        }

        pool.insert(pool.end(), routes.begin(), routes.end());
        std::sort(pool.begin(), pool.end(),
                  [](const auto& a, const auto& b){ return a.cost() < b.cost(); });
        if (pool.size() > best_count_) {
            pool.erase(pool.begin() + best_count_, pool.end());
        }

        iters_count_++;
        stagn++;
        cost_dynamics_.push_back(pool.front().cost());
        if (saturation_ && stagn >= saturation_) break;

        // сброс временного вектора эпохи
        std::vector<ScheduleStatus>().swap(routes);
    }

    Matrix().swap(matrix); // срез пика
    return best;
}

std::vector<std::vector<double>> AntColonySystem::makeMatrix(size_t n, double pad) const {
    return std::vector<std::vector<double>>(n, std::vector<double>(n, pad));
}

std::vector<ScheduleStatus> AntColonySystem::completeEpoch(const Graph &graph,
                                                           Matrix &matrix,
                                                           double init_trail)
{
    std::vector<ScheduleStatus> routes;
    routes.reserve(ants_.size()); // ВАЖНО: без resize в рост

    for (auto& ant : ants_) {
        auto route = ant.makeRoute(graph, matrix,
                                   phe_influence_, heu_influence_,
                                   threshold_, rng_);
        localPheUpdate(route, matrix, init_trail);
        routes.push_back(std::move(route));
    }

    std::sort(routes.begin(), routes.end(),
              [](const auto& a, const auto& b){ return a.cost() < b.cost(); });
    return routes;
}

AntColonySystem::AntColonySystem(BaseOptimization& baseline)
    : AntColonySystem(baseline,
                      /*evaporation   */ 0.1,
                      /*phe_decay     */ 0.2,
                      /*phe_influence */ 0.4,
                      /*heu_influence */ 0.6,
                      /*threshold     */ 0.9,
                      /*epochs_count  */ 10000u,
                      /*ants_count    */ 10u,
                      /*best_count    */ 3u,
                      /*saturation    */ 10000u,
                      /*improvement   */ 0.5,
                      /*seed          */ 42u,
                      /*label         */ "ACO")
{}

void AntColonySystem::localPheUpdate(const ScheduleStatus &status, Matrix &matrix, double delta) const {
    const double add = phe_decay_ * delta;
    const double keep = 1.0 - phe_decay_;
    for (size_t v = 1; v < status.size(); ++v) {
        for (size_t u = 0; u < v; ++u) {
            if (status.loc(u) < status.loc(v))
                matrix[u][v] = keep * matrix[u][v] + add;
            else
                matrix[v][u] = keep * matrix[v][u] + add;
        }
    }
}

void AntColonySystem::globalPheUpdate(const std::vector<ScheduleStatus> &epoch_routes,
                                      Matrix &matrix,
                                      double wmax) const
{
    for (auto& row : matrix)
        for (auto& x : row) x = (1.0 - evaporation_) * x;

    for (const auto& r : epoch_routes) {
        const double inc = evaporation_ * (double)wmax / std::max<weight_t>(1, r.cost());
        for (size_t u = 0; u < r.size(); ++u) {
            for (size_t v = u + 1; v < r.size(); ++v) {
                if (r.loc(u) < r.loc(v)) matrix[u][v] += inc;
                else                     matrix[v][u] += inc;
            }
        }
    }
}

std::unique_ptr<BaseOptimization> AntColonySystem::copy() const {
    auto aco = std::unique_ptr<AntColonySystem>(new AntColonySystem());
    aco->setParams(this->getParams());
    return aco;
}

void AntColonySystem::setParams(const ParamSet &p) {
    for (const auto& [k, v] : p) {
        if      (k == "evaporation")   evaporation_   = (double)v;
        else if (k == "phe_decay")     phe_decay_     = (double)v;
        else if (k == "phe_influence") phe_influence_ = (double)v;
        else if (k == "heu_influence") heu_influence_ = (double)v;
        else if (k == "threshold")     threshold_     = (double)v;
        else if (k == "epochs_count")  epochs_count_  = (unsigned)v;
        else if (k == "ants_count")    { ants_count_  = (unsigned)v; ants_.assign(ants_count_, ArtificialAnt{}); }
        else if (k == "best_count")    best_count_    = (unsigned)v;
        else                           IterativeOptimization::setParams({{k, v}});
    }
}

ParamSet AntColonySystem::getParams() const {
    auto p = IterativeOptimization::getParams();
    p["evaporation"]   = evaporation_;
    p["phe_decay"]     = phe_decay_;
    p["phe_influence"] = phe_influence_;
    p["heu_influence"] = heu_influence_;
    p["threshold"]     = threshold_;
    p["epochs_count"]  = epochs_count_;
    p["ants_count"]    = ants_count_;
    p["best_count"]    = best_count_;
    return p;
}

} // namespace scheduling_problem::algorithms
