#pragma once

#include <unordered_map>
#include "BaseOptimization.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * @brief Implements %greedy scheduling algorithm
     *
     * Makes a schedule, at each step choosing the task to insert
     * in such a way that the value of the goal function is the minimum
     * possible. The input graph must be
     * sorted topologically.
     */
    class Greedy : public virtual BaseOptimization
    {

    public:
        /**
         * @param label Algorithm name
         */
        Greedy(const std::string &label = "greedy");
        /**
         * Copy constructor
         */
        Greedy(const Greedy &other) = default;

        /**
         * @return Pointer to the object
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;

    protected:
        /**
         * Constructs a schedule for a given graph
         * @param graph Input graph
         * @return Constructed schedule
         */
        virtual Schedule schedule_(const Graph &graph);

        /**
         * @param graph Topologically sorted graph
         * @param schedule %Schedule for inserting the task
         * @param curr_vid Task node number
         * @return Possible positions for task
         */
        std::unordered_map<size_t, double> heuInfo(const Graph &graph,
                                                   const ScheduleStatus &schedule,
                                                   size_t curr_vid);

        /**
         * @param graph Topologically sorted graph
         * @param schedule %Schedule for inserting the task
         * @param curr_vid Task node number
         * @return Chosen  insert position
         */
        size_t choice(const Graph &graph,
                      const ScheduleStatus &schedule,
                      size_t curr_vid);
    };

}
