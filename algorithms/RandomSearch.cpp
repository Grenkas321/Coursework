#include "RandomSearch.h"
#include "ScheduleCorrector.h"
#include "TopologicalSort.h"


namespace scheduling_problem::algorithms
{
    /**
     * Construct a randomized iterative optimizer.
     *
     * @param max_iters    Maximum number of iterations/tries per run.
     * @param saturation   Early-stop after this many non-improving steps (0 disables).
     * @param improvement  Relative improvement threshold that resets stagnation.
     * @param seed         RNG seed.
     * @param label        Human-readable label.
     */
    RandomSearch::RandomSearch(unsigned max_iters,
                               unsigned saturation,
                               double improvement,
                               unsigned seed,
                               const std::string &label)
        : IterativeOptimization(BASELINE, saturation, improvement, seed, label), max_iters_(max_iters)
    {
    }

    /**
     * Iterative random search starting from a provided base schedule.
     *
     * The method:
     * 1) Initializes with the base schedule.
     * 2) Repeatedly applies `ScheduleCorrector::transform()` to produce a neighbor.
     * 3) Tracks the best schedule and a stagnation counter for early stopping.
     * 4) Records the cost dynamics.
     *
     * @param graph          Input task graph.
     * @param base_schedule  Initial schedule to refine.
     * @return               Best schedule found.
     */
    Schedule RandomSearch::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        ScheduleStatus status_base(graph, base_schedule);
        auto best_schedule = status_base;
        cost_dynamics_.clear();
        cost_dynamics_.push_back(best_schedule.cost());

        ScheduleStatus status(graph, best_schedule);
        ScheduleCorrector corrector(seed_);
        unsigned stagnations(0);

        for (iters_count_ = 0; iters_count_ < max_iters_; iters_count_++)
        {
            corrector.transform(graph, status);

            if (status.cost() < best_schedule.cost())
            {
                auto improvement = ((double)best_schedule.cost() - status.cost()) / best_schedule.cost();
                if (improvement > improvement_)
                    stagnations = 0;
                best_schedule = status;
            }

            stagnations++;
            if (saturation_ && stagnations >= saturation_)
                break;

            cost_dynamics_.push_back(status.cost());
        }
        return best_schedule;
    }

    /**
     * Choose a random feasible insertion position for `curr_vid`.
     *
     * @param graph    Input graph.
     * @param status   Current partial schedule.
     * @param curr_vid Vertex to insert.
     * @return         Chosen position in [lower, upper).
     */
    size_t RandomSearch::choice(const Graph &graph, const ScheduleStatus &status, size_t curr_vid)
    {
        auto lower(status.lower(curr_vid, graph)), upper(status.upper(curr_vid, graph));
        if (lower == upper)
            return upper;
        return std::uniform_int_distribution<size_t>(lower, upper - 1)(rng_);
    }

    /**
     * Polymorphic copy.
     *
     * @return New heap-allocated RandomSearch with the same configuration.
     */
    std::unique_ptr<BaseOptimization> RandomSearch::copy() const
    {
        return std::unique_ptr<RandomSearch>(new RandomSearch(max_iters_, saturation_, improvement_, seed_, label_));
    }

    /**
     * Set RandomSearch-specific parameters.
     * Recognized key:
     *  - "max_iters" (unsigned)
     *
     * Note: other keys are intentionally ignored here; they can be set via
     * the IterativeOptimization/BaseOptimization interfaces.
     *
     * @param params  Parameter map.
     */
    void RandomSearch::setParams(const ParamSet &params)
    {
        for (auto [param, val] : params)
        {
            if (param == "max_iters")
                max_iters_ = (unsigned)val;
        }
    }

    /**
     * Get current parameters including inherited ones.
     *
     * @return Parameter set with "label", "saturation", "improvement", "seed", "max_iters".
     */
    ParamSet RandomSearch::getParams() const
    {
        auto params = IterativeOptimization::getParams();
        params["max_iters"] = max_iters_;
        return params;
    }

    /**
     * Generate a random schedule by inserting vertices in random feasible positions.
     *
     * Runs up to `max_iters_` attempts and returns the best schedule encountered.
     *
     * @param graph  Input task graph.
     * @return       Best randomly generated schedule.
     */
    Schedule RandomSearch::generateRandomSchedule(const Graph &graph)
{
    size_t n_vertex = boost::num_vertices(graph);
    ScheduleStatus status(graph);

    using scheduling_problem::topo_sort;
    auto topo = topo_sort(graph);

    if (topo.size() != n_vertex) {
        return {};  // граф содержит цикл
    }

    std::shuffle(topo.begin(), topo.end(), rng_);

    for (size_t curr_vid : topo)
    {
        size_t low  = status.lower(curr_vid, graph);
        size_t high = status.upper(curr_vid, graph);

        size_t pos;
        if (low >= high) {
            // 🛡 Защита: если нет допустимого диапазона, вставим в конец или безопасную границу
            pos = std::min(low, status.size());
        } else {
            std::uniform_int_distribution<size_t> dist(low, high - 1);
            pos = dist(rng_);
        }

        // 🛡 Дополнительная защита: не вставляем за пределы
        pos = std::min(pos, status.size());

        status.insert(curr_vid, pos, graph);
    }

    return static_cast<Schedule>(status);
}

}
