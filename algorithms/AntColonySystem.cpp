#include "AntColonySystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include "TopologicalSort.h"
#include <iostream>

namespace scheduling_problem::algorithms {

ScheduleStatus AntColonySystem::ArtificialAnt::makeRoute(
    const Graph &graph,
    Matrix &matrix,
    double phe_influence,
    double heu_influence,
    double threshold,
    randgen &rng)
{
    // --- ТОПОЛОГИЧЕСКИЙ ПОРЯДОК ---
    auto order = scheduling_problem::topo_sort(graph);

    // если граф не DAG — возвращаем пустое расписание
    if (order.size() != boost::num_vertices(graph))
        return ScheduleStatus(graph);

    ScheduleStatus status(graph);
    status.reset_positions();

    // вставляем корень
    status.insert(order[0], 0, graph);

    for (size_t i = 1; i < order.size(); ++i)
    {
        size_t v = order[i];

        auto desir = heuInfo(graph, status, v);

        // --- страховка, если нарушены границы (desir пустой) ---
        if (desir.empty())
        {
            size_t low  = status.lower(v, graph);
            size_t high = status.upper(v, graph);

            size_t safe_pos = std::min(low, high);
            if (safe_pos > status.size())
                safe_pos = status.size();

            status.insert(v, safe_pos, graph);
            continue;
        }

        // нормальный выбор позиции
        size_t pos = choice(graph, matrix, status, v,
                            phe_influence, heu_influence, threshold, rng);

        if (pos > status.size()) pos = status.size();

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
    auto desir = heuInfo(graph, status, curr_vid);
    // Если нет допустимых позиций для вставки вершины – возвращаем вставку в конец 
    if (desir.empty()) {
        return status.size();
    }
    for (auto& [pos, heu_val] : desir) {
        double tau = 1.0;
        // Учитываем феромоны относительно всех уже запланированных вершин
        for (size_t idx = 0; idx < status.size(); ++idx) {
            size_t u = status[idx].id;
            if (status.loc(u) < pos)
                tau *= matrix[u][curr_vid];
            else
                tau *= matrix[curr_vid][u];
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
        // fallback: выбираем первую позицию, если суммы некорректны
        return desir.begin()->first;
    }
    std::uniform_real_distribution<double> R(0.0, sum);
    double t = R(rng);
    for (const auto& kv : desir) {
        if ((t -= kv.second) <= 0.0) {
            return kv.first;
        }
    }
    return desir.begin()->first;
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
    // Проверка на наличие циклов в графе (входной граф должен быть DAG)
    const auto total_vertices = boost::num_vertices(graph);
    auto topo_order = scheduling_problem::topo_sort(graph);
    if (topo_order.size() != total_vertices) {
        std::cerr << "Error: input graph contains a cycle or is not a DAG. Terminating ACO.\n";
        return Schedule();
    }

    // 1. Стартовая информация о графе
    weight_t weight_max = 0;
    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
        if (boost::get(vertex_weight_t(), graph, vertex) > weight_max)
            weight_max = boost::get(vertex_weight_t(), graph, vertex);

    auto n_vertices   = boost::num_vertices(graph);
    auto best_solution = base_schedule;

    // На всякий случай: если вдруг базовое расписание пустое или с нулевой ценой --
    // пересчитаем его через baseline (Greedy).
    if (!best_solution.size() || best_solution.cost() <= 0)
    {
        auto fallback = baseline_->schedule(graph);
        if (fallback.size() && fallback.cost() > 0)
            best_solution = fallback;
    }

    auto denom = best_solution.cost();
    if (denom <= 0) denom = 1;  // жёсткая защита от деления на ноль

    double init_trail = static_cast<double>(weight_max) / static_cast<double>(denom);
    auto   weight_matrix = makeMatrix(n_vertices, init_trail);

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
        // Глобальное обновление феромона по текущему пулу
        globalPheUpdate(solution_pool, weight_matrix, weight_max);

        // Один “прогон” всех муравьёв
        auto epoch_routes = completeEpoch(graph, weight_matrix, init_trail);

        // Жёсткая проверка на пустой результат, чтобы не словить epoch_routes[0]
        if (epoch_routes.empty())
            break;

        // Улучшилось ли лучшее решение?
        if (epoch_routes.front().cost() < best_solution.cost())
        {
            auto improvement = (double)(best_solution.cost() - epoch_routes.front().cost())
                             / (double)best_solution.cost();

            if (improvement > improvement_)
                stagnations = 0;

            best_solution = epoch_routes.front();
        }

        // Обновляем общий пул решений
        solution_pool.insert(solution_pool.end(), epoch_routes.begin(), epoch_routes.end());
        std::sort(solution_pool.begin(), solution_pool.end(),
                  [](const auto &a, const auto &b){ return a.cost() < b.cost(); });

        if (solution_pool.size() > best_count_)
            solution_pool.erase(solution_pool.begin() + best_count_, solution_pool.end());

        iters_count_++;
        stagnations++;
        cost_dynamics_.push_back(epoch_routes.front().cost());

        if (saturation_ && stagnations >= saturation_)
            break;
    }

    return best_solution;
}

std::vector<std::vector<double>> AntColonySystem::makeMatrix(size_t n, double pad) const {
    return std::vector<std::vector<double>>(n, std::vector<double>(n, pad));
}

std::vector<ScheduleStatus> AntColonySystem::completeEpoch(const Graph &graph,
                                                           Matrix &matrix,
                                                           double init_trail)
{
    std::vector<ScheduleStatus> routes;
    routes.reserve(ants_.size());

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
                      0.1,   /* evaporation   */
                      0.2,   /* phe_decay     */
                      0.4,   /* phe_influence */
                      0.6,   /* heu_influence */
                      0.9,   /* threshold     */
                      10000u,/* epochs_count  */
                      10u,   /* ants_count    */
                      3u,    /* best_count    */
                      10000u,/* saturation    */
                      0.5,   /* improvement   */
                      42u,   /* seed          */
                      "ACO") /* label         */
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
