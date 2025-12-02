#pragma once

#include "IterativeOptimization.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * RandomSearch — randomized iterative scheduler.
     *
     * Repeatedly perturbs a schedule (optionally starting from a baseline),
     * keeps the best result, and supports early stopping via saturation/
     * improvement thresholds inherited from IterativeOptimization.
     */
    class RandomSearch : public IterativeOptimization
    {
    protected:
        /** Maximum number of iterations/tries performed per run. */
        unsigned max_iters_;

    public:
        /**
         * Construct a RandomSearch optimizer.
         *
         * @param max_iters   Maximum iterations per run (default: 100).
         * @param saturation  Stop after this many non-improving steps (0 disables).
         * @param improvement Relative improvement threshold to reset stagnation.
         * @param seed        RNG seed (default: 42).
         * @param label       Human-readable label (default: "rsearch").
         */
        RandomSearch(unsigned max_iters = 100,
                     unsigned saturation = 0,
                     double improvement = 0.0,
                     unsigned seed = 42,
                     const std::string &label = "rsearch");

        /**
         * Copy constructor.
         */
        RandomSearch(const RandomSearch &other) = default;

        /**
         * Polymorphic copy.
         *
         * @return New heap-allocated optimizer with the same configuration.
         */
        std::unique_ptr<BaseOptimization> copy() const override;

        /**
         * Set RandomSearch-specific parameters.
         *
         * Recognized keys:
         *  - "max_iters" (unsigned)
         * Other keys are handled by IterativeOptimization/BaseOptimization.
         *
         * @param params  Parameter set.
         */
        void setParams(const ParamSet &params) override;

        /**
         * Get current parameters including inherited ones.
         *
         * @return Parameter set with keys: "label", "saturation",
         *         "improvement", "seed", and "max_iters".
         */
        ParamSet getParams() const override;

        /**
         * Generate a random schedule from scratch.
         *
         * Inserts each vertex at a random feasible position and repeats up to
         * @c max_iters_ times, returning the best schedule encountered.
         *
         * @param graph  Input task graph.
         * @return       Best randomly generated schedule.
         */
        Schedule generateRandomSchedule(const Graph &graph);

    protected:
        /**
         * Iterative routine starting from a given baseline schedule.
         *
         * Applies randomized transformations to improve the provided
         * @p base_schedule, tracking best cost and respecting early-stop rules.
         *
         * @param graph          Input task graph.
         * @param base_schedule  Initial schedule to refine.
         * @return               Best schedule found.
         */
        Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;

        /**
         * Choose a random feasible insertion position for a vertex.
         *
         * @param graph     Input task graph.
         * @param status    Current partial schedule.
         * @param curr_vid  Vertex id to insert.
         * @return          Position in the half-open interval [lower, upper).
         */
        size_t choice(const Graph &graph, const ScheduleStatus &status, size_t curr_vid);
    };

}
