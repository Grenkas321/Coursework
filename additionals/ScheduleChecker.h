#pragma once

#include <string>
#include "general_types.h"
#include "Schedule.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Validator for schedules under the “edge-buffer” memory model.
     *
     * Model assumptions:
     *  - A buffer is a group of edges (parent -> child) that share the same pair (parent, buffer_id).
     *  - Buffer size is stored in edge_weight; the group id is stored in edge_buffer_id_t.
     *  - Memory “raises” when the producer vertex is executed (amount equals the sum of its buffer sizes).
     *  - A buffer is released when its last consumer is executed.
     */
    class ScheduleChecker
    {
    public:
        /**
         * @brief Full consistency check with respect to buffers.
         *
         * Verifies, for every schedule position, that the computed release (based on
         * last-consumer semantics per buffer) equals the recorded Job::release and
         * that invariants hold.
         *
         * @param graph   Input graph.
         * @param sched   Schedule to verify (Schedule or ScheduleStatus).
         * @param message Optional pointer to receive a human-readable error description.
         * @return true if the schedule is valid; false otherwise.
         */
        static bool isCorrect(const Graph& graph,
                              const Schedule& sched,
                              std::string* message = nullptr);

        /**
         * @brief Recompute the peak in-flight memory according to the buffer model.
         *
         * Useful for comparison with Schedule::cost(graph), if needed.
         *
         * @param graph Input graph.
         * @param sched Schedule to analyze.
         * @return Peak memory usage observed while scanning the schedule.
         */
        static weight_t recomputePeak(const Graph& graph, const Schedule& sched);
    };
}
