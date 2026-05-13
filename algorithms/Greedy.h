#pragma once

#include <limits>

#include <unordered_map>

#include "BaseOptimization.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Deterministic greedy list scheduler:
     * - chooses the next task by earliest feasible start time,
     * - places into the earliest processor hole that fits dependencies,
     * - enforces a hard memory limit.
     */
    class Greedy : public virtual BaseOptimization
    {
    private:
        /** Number of processors for placement. */
        unsigned processors_ = 1;
        /** Memory limit (LLONG_MAX means unlimited). */
        weight_t memory_limit_ = std::numeric_limits<weight_t>::max();

    public:
        explicit Greedy(const std::string &label = "greedy");
        Greedy(const Greedy &other) = default;

        std::unique_ptr<BaseOptimization> copy() const override;

        /**
         * Recognized params:
         * - processors (unsigned)
         * - memory_limit (number)
         */
        void setParams(const ParamSet &params) override;

        ParamSet getParams() const override;

    protected:
        Schedule schedule_(const Graph &graph) override;

        /**
         * Backward-compatible heuristic API used by ACO ants.
         * Returns desirability (higher is better) for each insertion position.
         */
        std::unordered_map<size_t, double> heuInfo(const Graph &graph,
                                                   const ScheduleStatus &schedule,
                                                   size_t curr_vid);

        /**
         * Choose insertion position by maximal desirability.
         */
        size_t choice(const Graph &graph,
                      const ScheduleStatus &schedule,
                      size_t curr_vid);
    };
    
    class Greedy2 : public virtual BaseOptimization
    {
    private:
        /** Number of processors for placement. */
        unsigned processors_ = 1;
        /** Memory limit (LLONG_MAX means unlimited). */
        weight_t memory_limit_ = std::numeric_limits<weight_t>::max();

    public:
        explicit Greedy2(const std::string &label = "greedy2");
        Greedy2(const Greedy2 &other) = default;

        std::unique_ptr<BaseOptimization> copy() const override;

        /**
         * Recognized params:
         * - processors (unsigned)
         * - memory_limit (number)
         */
        void setParams(const ParamSet &params) override;

        ParamSet getParams() const override;

    protected:
        Schedule schedule_(const Graph &graph) override;

        /**
         * Backward-compatible heuristic API used by ACO ants.
         * Returns desirability (higher is better) for each insertion position.
         */
        std::unordered_map<size_t, double> heuInfo(const Graph &graph,
                                                   const ScheduleStatus &schedule,
                                                   size_t curr_vid);

        /**
         * Choose insertion position by maximal desirability.
         */
        size_t choice(const Graph &graph,
                      const ScheduleStatus &schedule,
                      size_t curr_vid);
    };
}
