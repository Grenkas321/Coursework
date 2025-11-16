#include "ConcurrentSAO.h"
#include <algorithm>
#include <numeric>

namespace scheduling_problem::algorithms {

ConcurrentSAO::ConcurrentSAO(unsigned partitions_count,
                             unsigned rsearch_iters,
                             bool warm_start,
                             bool subareas,
                             double min_temp,
                             double max_temp,
                             SimulatedAnnealing::ReductionRules reduction_rule,
                             unsigned saturation,
                             double improvement,
                             unsigned seed,
                             const std::string& label)
    : BaseOptimization(label)
    , partitions_count_(partitions_count)
    , rsearch_iters_(rsearch_iters)
    , warm_start_(warm_start)
    , subareas_(subareas)
    , rng_(seed)
{
    algo_params_.clear();
    algo_params_.emplace("min_temp",       parameter(min_temp));
    algo_params_.emplace("max_temp",       parameter(max_temp));
    algo_params_.emplace("reduction_rule", parameter((unsigned)reduction_rule));
    algo_params_.emplace("saturation",     parameter(saturation));
    algo_params_.emplace("improvement",    parameter(improvement));
    // безопасные заглушки
    algo_params_.emplace("alpha",          parameter(1.0));
    algo_params_.emplace("beta",           parameter(1.0));
    algo_params_.emplace("start_temp",     parameter(min_temp));
    algo_params_.emplace("end_temp",       parameter(max_temp));
}

ConcurrentSAO::~ConcurrentSAO() = default;

std::unique_ptr<BaseOptimization> ConcurrentSAO::copy() const {
    return std::unique_ptr<BaseOptimization>(new ConcurrentSAO(*this));
}

static double getd(const ConcurrentSAO::ParamSet& p, const char* k, double def) {
    auto it = p.find(k); return it==p.end()?def:(double)it->second;
}
static unsigned getu(const ConcurrentSAO::ParamSet& p, const char* k, unsigned def) {
    auto it = p.find(k); return it==p.end()?def:(unsigned)it->second;
}

void ConcurrentSAO::setParams(const ParamSet& params) {
    for (const auto& [k,v] : params) {
        if      (k == "partitions_count") partitions_count_ = (unsigned)v;
        else if (k == "rsearch_iters")    rsearch_iters_    = (unsigned)v;
        else if (k == "warm_start")       warm_start_       = (bool)v;
        else if (k == "subareas")         subareas_         = (bool)v;
        else                              algo_params_[k]   = v;
    }
}

ConcurrentSAO::ParamSet ConcurrentSAO::getParams() const {
    auto p = algo_params_;
    p["partitions_count"] = partitions_count_;
    p["rsearch_iters"]    = rsearch_iters_;
    p["warm_start"]       = warm_start_;
    p["subareas"]         = subareas_;
    return p;
}

ConcurrentSAO::ConcurrentSAO()
    : ConcurrentSAO(/*partitions_count*/ 3u,
                    /*rsearch_iters   */ 20u,
                    /*warm_start      */ true,
                    /*subareas        */ true,
                    /*min_temp        */ 1.0,
                    /*max_temp        */ 5.0,
                    /*reduction_rule  */ static_cast<SimulatedAnnealing::ReductionRules>(0),
                    /*saturation      */ 0u,
                    /*improvement     */ 0.0,
                    /*seed            */ 42u,
                    /*label           */ "CSAO")
{}


Schedule ConcurrentSAO::schedule_(const Graph& graph)
{
    auto baselines = std::make_shared<std::vector<Schedule>>();
    baselines->reserve(partitions_count_);

    if (warm_start_) {
        Greedy g;
        baselines->push_back(g.schedule(graph));
    }

    RandomSearch rs(3000);
    while (baselines->size() < partitions_count_) {
        baselines->push_back(rs.generateRandomSchedule(graph));
    }

    auto pool = wave_(graph, baselines, partitions_count_);

    const unsigned saturation = getu(algo_params_, "saturation", 0u);
    const double   improvement = getd(algo_params_, "improvement", 0.0);
    unsigned stagn = 0;
    weight_t best_cost = pool.front().cost();

    // Несколько волн — без тяжелого SA, зато стабильно и без зависимостей
    for (unsigned epoch = 0; epoch < 8; ++epoch) {
        auto next_base = std::make_shared<std::vector<Schedule>>();
        next_base->reserve(partitions_count_);
        for (size_t i = 0; i < pool.size() && next_base->size() < partitions_count_; ++i) {
            next_base->push_back( Schedule(pool[i]) );
        }

        auto next_pool = wave_(graph, next_base, partitions_count_);

        if (next_pool.front().cost() + (weight_t)improvement < best_cost) {
            best_cost = next_pool.front().cost();
            stagn = 0;
        } else {
            if (saturation && ++stagn >= saturation) break;
        }

        pool.swap(next_pool);
        std::vector<ScheduleStatus>().swap(next_pool); // срез пика
    }

    Schedule best = pool.front();
    std::vector<ScheduleStatus>().swap(pool);

    const size_t n = boost::num_vertices(graph);
    conveyor.clear();
    conveyor.emplace_back();
    auto& row = conveyor.back();
    row.resize(n);
    for (size_t i = 0; i < n; ++i) row[i] = static_cast<long long>(i);

    return best;
}

std::vector<ScheduleStatus>
ConcurrentSAO::wave_(const Graph& graph,
                     std::shared_ptr<std::vector<Schedule>> baselines,
                     unsigned keep_top)
{
    std::vector<ScheduleStatus> pool;
    pool.reserve(baselines->size()); // ВАЖНО: без resize в рост

    for (const auto& base : *baselines) {
        ScheduleStatus s(graph, base);
        // лёгкая локальная перестановка (без SA), чтобы разнообразить
        if (s.size() >= 2) {
            const size_t a = 1 % s.size();
            const size_t to = s.lower(a, graph);
            s.move(a, to, graph);
        }
        pool.emplace_back(std::move(s));
    }

    std::sort(pool.begin(), pool.end(),
              [](const auto& A, const auto& B){ return A.cost() < B.cost(); });
    if (pool.size() > keep_top) {
        pool.erase(pool.begin() + keep_top, pool.end());
    }

    baselines.reset(); // общий буфер волны больше не нужен — срез пика
    return pool;
}

} // namespace scheduling_problem::algorithms
