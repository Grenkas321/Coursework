#include "IterativeOptimization.h"

namespace scheduling_problem::algorithms
{
    /**
     * Construct an iterative optimizer that wraps a baseline optimizer.
     *
     * @param baseline    Baseline algorithm used to obtain an initial schedule.
     * @param saturation  Max number of non-improving iterations before early stop (0 disables).
     * @param improvement Relative improvement threshold that resets stagnation counter.
     * @param seed        RNG seed used by iterative procedures.
     * @param label       Human-readable label for this optimizer.
     */
    IterativeOptimization::IterativeOptimization(const BaseOptimization &baseline,
                                                 unsigned saturation,
                                                 double improvement,
                                                 unsigned seed,
                                                 const std::string &label)
        : BaseOptimization(label), baseline_(std::move(baseline.copy())), saturation_(saturation), improvement_(improvement), seed_(seed), rng_(seed), iters_count_(0)
    {
    }

    /**
     * Run iterative scheduling starting from a baseline schedule produced by `baseline_`.
     * Resets the cost dynamics trace.
     *
     * @param graph  Input task graph.
     * @return       Best schedule found by the iterative routine.
     */
    Schedule IterativeOptimization::schedule_(const Graph &graph)
    {
        cost_dynamics_.clear();
        auto base_schedule = baseline_->schedule(graph);
        auto solution = schedule_(graph, base_schedule);

        return solution;
    }

    /**
     * Timed overload: run the iterative routine starting from a provided base schedule.
     * Resets the cost dynamics trace and measures wall-clock duration.
     *
     * @param graph          Input task graph.
     * @param base_schedule  Initial schedule to refine.
     * @return               Best schedule found.
     */
    Schedule IterativeOptimization::schedule(const Graph &graph, const Schedule &base_schedule)
    {
        cost_dynamics_.clear();
        auto start = currentTime();
        auto solution = schedule_(graph, base_schedule);
        auto stop = currentTime();
        duration({start, stop});
        return solution;
    }

    /**
     * Get the number of iterations performed in the last run.
     *
     * @return Iteration count.
     */
    unsigned IterativeOptimization::itersCount() const
    {
        return iters_count_;
    }

    /**
     * Get the recorded objective dynamics for the last run.
     *
     * @return Vector of objective values collected during the last run.
     */
    std::vector<scheduling_problem::weight_t> IterativeOptimization::costDynamics() const
    {
        return cost_dynamics_;
    }

    /**
     * Set iterative-optimizer parameters from a key-value map.
     * Recognized keys:
     *  - "saturation"  (unsigned)
     *  - "improvement" (double)
     *  - "seed"        (unsigned) — also resets the internal RNG
     * Other keys are forwarded to BaseOptimization::setParams.
     *
     * @param params  Parameter set.
     */
    void IterativeOptimization::setParams(const ParamSet &params)
    {
        for (const auto &param : params)
        {
            if (param.first == "saturation")
            {
                saturation_ = (unsigned)param.second;
            }
            else if (param.first == "improvement")
            {
                improvement_ = param.second;
            }
            else if (param.first == "seed")
            {
                seed_ = param.second;
                rng_ = randgen(seed_);
            }
            else
            {
                BaseOptimization::setParams({param});
            }
        }
    }

    /**
     * Get the current parameters as a key-value map, including
     * base class fields and iterative-specific ones.
     *
     * @return Parameter set with keys: "label", "saturation", "improvement", "seed".
     */
    ParamSet IterativeOptimization::getParams() const
    {
        auto params = BaseOptimization::getParams();
        params["saturation"] = saturation_;
        params["improvement"] = improvement_;
        params["seed"] = seed_;
        return params;
    }
}
