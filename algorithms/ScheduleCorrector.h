#pragma once

#include <random>
#include <vector>
#include "general_types.h"
#include "ScheduleStatus.h"
#include <limits>


namespace scheduling_problem::algorithms
{
    /**
     * ScheduleCorrector — lightweight local schedule editor.
     *
     * Applies small randomized moves (vertex reinsertions within feasible
     * precedence bounds) to explore nearby schedules. Each successful call to
     * transform() stores enough context to optionally revert the move via
     * invtransform().
     */
    class ScheduleCorrector
    {
        static constexpr size_t kInvalid = std::numeric_limits<size_t>::max();

    protected:
        /** Pseudorandom generator used for move selection. */
        std::mt19937 rng_;
        /** Id of the last moved vertex (or (size_t)-1 if none). */
        size_t moved_vid_;
        /** Previous position of the last moved vertex. */
        size_t last_pos_;
        /** If true, record the sequence of applied moves in `trail_`. */
        bool track_trail_;
        /** Optional trail of moves: (vertex_id, previous_position). */
        std::vector<std::pair<size_t, size_t>> trail_;

    public:
        /**
         * Construct a schedule corrector.
         *
         * @param seed         RNG seed (default: 42).
         * @param track_trail  Whether to record the move trail (default: false).
         */
        ScheduleCorrector(unsigned seed = 42, bool track_trail = false);

        /**
         * Apply a single randomized feasible move.
         *
         * Picks a movable vertex and reinserts it at a randomly selected
         * admissible position [lower, upper) that differs from the current one.
         * Stores enough state to allow a subsequent invtransform().
         *
         * @param graph   Input task graph.
         * @param status  Schedule to modify in place.
         * @return        Current schedule cost after the move.
         */
        weight_t transform(const Graph &graph, ScheduleStatus &status);

        /**
         * Revert the last transform() move if still valid.
         *
         * If the previously moved vertex is still present and its saved target
         * position is within the current admissible bounds, move it back.
         *
         * @param graph   Input task graph.
         * @param status  Schedule to modify in place.
         * @return        Current schedule cost after the revert (or no-op).
         */
        weight_t invtransform(const Graph &graph, ScheduleStatus &status);

    private:
        /**
         * Randomly choose a movable vertex and a new target position.
         *
         * A vertex is considered movable if upper(id) > lower(id) + 1.
         * Returns {-1, -1} if no vertex is movable.
         *
         * @param graph    Input task graph.
         * @param status   Current schedule (read-only).
         * @return         Pair {vertex_id, target_pos}.
         */
        std::pair<size_t, size_t> choice(const Graph &graph, const ScheduleStatus &status);
    };
}
