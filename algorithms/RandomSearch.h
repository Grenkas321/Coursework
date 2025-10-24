#pragma once

#include "IterativeOptimization.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Random search scheduling algorithm
     */
    class RandomSearch : public IterativeOptimization
    {
    protected:
        unsigned max_iters_;

    public:
        /**
         * Class constructor
         */
        RandomSearch(unsigned max_iters = 100,
                     unsigned saturation = 0,
                     double improvement = 0.0,
                     unsigned seed = 42,
                     const std::string &label = "rsearch");
        /**
         * Copy constructor
         */
        RandomSearch(const RandomSearch &other) = default;
        /**
         * Copy class instance
         */
        virtual std::unique_ptr<BaseOptimization> copy() const;
        /**
         * Sets internal algorithm parameters
         */
        virtual void setParams(const ParamSet &params);
        /**
         * Get internal parameters
         */
        virtual ParamSet getParams() const;

        /**
         * Create random schedule for the given graph
         */
        Schedule generateRandomSchedule(const Graph &graph);

    protected:
        /**
         * Constructs a schedule for a given graph and a given baseline schedule
         */
        virtual Schedule schedule_(const Graph &graph, const Schedule &baseline);
        /**
         * Choose next schedule according to input graph and current schedule
         */
        size_t choice(const Graph &graph, const ScheduleStatus &status, size_t curr_vid);
    };

}
