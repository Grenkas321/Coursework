#pragma once

#include <vector>
#include <limits>

#include "general_types.h"
#include "Schedule.h"

namespace scheduling_problem::algorithms
{
    /**
     * Layered schedule state: per-processor ordered chains of tasks.
     */
    struct LayeredState
    {
        std::vector<std::vector<size_t>> processors;
        std::vector<unsigned> proc_of;
        std::vector<size_t> tier_of;
    };

    /**
     * Evaluation of a layered state under:
     * - precedence constraints,
     * - processor serial constraints,
     * - memory limit.
     */
    struct LayeredEval
    {
        std::vector<weight_t> start;
        std::vector<weight_t> finish;
        weight_t makespan = 0;
        weight_t peak_memory = 0;
        weight_t overflow = 0;
        bool acyclic = true;

        bool feasible() const
        {
            return acyclic && overflow == 0;
        }

        double score(double overflow_penalty) const
        {
            if (!acyclic)
                return std::numeric_limits<double>::max() / 4.0;
            return static_cast<double>(makespan) + overflow_penalty * static_cast<double>(overflow);
        }
    };

    /**
     * Build a simple list-based multiprocessor initial state.
     */
    LayeredState makeListState(const Graph &graph, unsigned processors);

    /**
     * Convert a linear schedule into an initial layered state.
     */
    LayeredState makeStateFromSchedule(const Graph &graph, const Schedule &schedule, unsigned processors);

    /**
     * Evaluate layered state: earliest schedule + memory events.
     */
    LayeredEval evaluateLayeredState(const Graph &graph,
                                     const LayeredState &state,
                                     weight_t memory_limit = std::numeric_limits<weight_t>::max());

    /**
     * Convert evaluated layered state to common Schedule object.
     */
    Schedule toSchedule(const Graph &graph, const LayeredState &state, const LayeredEval &eval);

    /**
     * Get current tier (position in processor chain) of a task.
     */
    size_t taskTier(const LayeredState &state, size_t task);

    /**
     * O1-like operation: move task to another processor, preserving tier index if possible.
     */
    bool moveTaskToProcessor(LayeredState &state, size_t task, unsigned new_proc, size_t target_tier);

    /**
     * O2-like operation: move task to another tier on the same processor.
     */
    bool moveTaskToTier(LayeredState &state, size_t task, size_t target_tier);
}

