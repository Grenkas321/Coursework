#pragma once

#include <boost/range/iterator_range.hpp>
#include "Schedule.h"

namespace scheduling_problem
{
    /**
     * ScheduleStatus: a schedule with fast positional queries and edits.
     *
     * Provides:
     * - O(1) lookup of a vertex position by id (loc),
     * - insertion and in-place moves with position tracking,
     * - parent/child queries (by real edges only unless specified),
     * - helpers to reason about buffer releases.
     *
     * Unless stated otherwise, methods consider only real edges (EdgeKind::Real).
     */
    class ScheduleStatus : public Schedule
    {
        /** Position of each vertex id in the schedule (0 if unknown/first, see contains()). */
        std::vector<size_t> positions_;
        /** Remaining real children count per vertex (used in high-density mode). */
        std::vector<unsigned> child_remain_;
        /** Pointers to the position where a parent's buffer releases (high-density mode). */
        std::vector<size_t *> release_on_;
        /** Enable external-memory helpers for dense graphs to speed up moves. */
        bool use_external_mem_;
        static constexpr size_t NOT_PRESENT = size_t(-1);

    public:
        /**
         * Construct an empty status bound to a graph.
         * Optionally enables high-density helpers for faster move updates.
         *
         * @param graph         Task graph.
         * @param high_density  If true, allocate auxiliary arrays for fast updates.
         */
        ScheduleStatus(const Graph &graph, bool high_density = false);

        /**
         * Construct a status from an existing schedule and graph.
         * Rebuilds positions, per-job releases, and cost consistently.
         *
         * @param graph         Task graph.
         * @param schedule      Existing schedule to import.
         * @param high_density  If true, allocate auxiliary arrays for fast updates.
         */
        ScheduleStatus(const Graph &graph, const Schedule &schedule, bool high_density = false);

        /**
         * Copy constructor: duplicates schedule, positions, and helpers.
         *
         * @param other  Source status.
         */
        ScheduleStatus(const ScheduleStatus &other);

        /**
         * Defaulted copy assignment.
         */
        ScheduleStatus& operator=(const ScheduleStatus&) = default;

        /**
         * Get the current position of a vertex in the schedule.
         *
         * @param curr_vid  Vertex id.
         * @return 0-based position.
         */
        size_t loc(size_t curr_vid) const;

        /**
         * Compute the exclusive upper bound for placing a vertex.
         * A vertex cannot be scheduled after its earliest scheduled child.
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Exclusive upper bound in [1..size()], or size() if unconstrained.
         */
        size_t upper(size_t curr_vid, const Graph &graph) const;

        /**
         * Compute the inclusive lower bound for placing a vertex.
         * A vertex must be placed strictly after all of its scheduled parents.
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Inclusive lower bound in [0..size()].
         */
        size_t lower(size_t curr_vid, const Graph &graph) const;

        /**
         * For a vertex, report how many real children are not yet scheduled
         * and the last position among already scheduled children.
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return {remaining_children_count, last_child_position}.
         */
        std::pair<size_t, size_t> releaseOn(size_t curr_vid, const Graph &graph) const;

        /**
         * Check whether a vertex is already present in the schedule.
         * Note: position 0 can mean "absent" or "at index 0"; disambiguated via front().
         *
         * @param curr_vid  Vertex id.
         * @return True if present, false otherwise.
         */
        bool contains(size_t curr_vid) const;

        /**
         * List real parents of a vertex (EdgeKind::Real).
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Vector of parent ids.
         */
        std::vector<size_t> parents(size_t curr_vid, const Graph &graph) const;

        /**
         * List real children of a vertex (EdgeKind::Real).
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Vector of child ids.
         */
        std::vector<size_t> children(size_t curr_vid, const Graph &graph) const;

        /**
         * Fast parent iterable over all incoming edges (including Imaginary).
         * Caller is responsible for any filtering by kind.
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Iterator range of inverse adjacency.
         */
        boost::iterator_range<DiGraph::inv_adjacency_iterator>
        parentsf(size_t curr_vid, const Graph &graph) const;

        /**
         * Fast children iterable over all outgoing edges (including Imaginary).
         * Caller is responsible for any filtering by kind.
         *
         * @param curr_vid  Vertex id.
         * @param graph     Task graph.
         * @return Iterator range of adjacency.
         */
        boost::iterator_range<DiGraph::adjacency_iterator>
        childrenf(size_t curr_vid, const Graph &graph) const;

        /**
         * Insert a vertex as a job at a given position.
         * Recomputes releases and cost using the graph afterwards.
         *
         * @param curr_vid  Vertex id to insert.
         * @param pos       Target position (0-based).
         * @param graph     Task graph.
         */
        void insert(size_t curr_vid, size_t pos, const Graph &graph);

        /**
         * Move an already scheduled vertex to a new position.
         * Performs rotation, fixes positional indices, then recomputes cost.
         *
         * @param curr_vid  Vertex id to move.
         * @param tpos      Target position (0-based).
         * @param graph     Task graph.
         * @return New schedule cost after the move.
         */
        weight_t move(size_t curr_vid, size_t tpos, const Graph &graph);

        void reset_positions() {
            for (auto& p : positions_) p = NOT_PRESENT;
        }

        /**
         * Random access to a job by position (mutable).
         *
         * @param pos  Position index.
         * @return Reference to job at pos.
         */
        Job& operator[](size_t pos);

        /**
         * Random access to a job by position (const).
         *
         * @param pos  Position index.
         * @return Const reference to job at pos.
         */
        const Job& operator[](size_t pos) const;

        /**
         * Current number of scheduled jobs.
         *
         * @return Schedule length.
         */
        size_t size() const;

    private:
        /**
         * Update the positions_ array for a contiguous range after rotation.
         *
         * @param spos  Start position (inclusive).
         * @param fpos  Final position (inclusive).
         */
        inline void updatePositions(size_t spos, size_t fpos);

        /**
         * Compute the release position for a parent, assuming all real children
         * are already scheduled. If some child is missing, returns {false, 0}.
         *
         * @param curr_vid  Parent vertex id.
         * @param graph     Task graph.
         * @return {is_releasable, release_pos}.
         */
        inline std::pair<bool, size_t> releasePosition(size_t curr_vid, const Graph &graph) const;

        /**
         * Compute the previous release position of a parent relative to target_pos:
         * the maximum child position different from target_pos. If a child is
         * missing, returns {false, 0}.
         *
         * @param curr_vid    Parent vertex id.
         * @param graph       Task graph.
         * @param target_pos  Position to exclude.
         * @return {is_releasable, prev_release_pos}.
         */
        inline std::pair<bool, size_t> prevReleasePosition(size_t curr_vid, const Graph &graph, size_t target_pos) const;

        /**
         * Move a vertex left (toward the beginning) via reverse-iterator rotation.
         * Adjusts release redistribution and helper pointers as needed.
         *
         * @param curr_vid   Vertex id being moved.
         * @param curr_pos   Current position.
         * @param target_pos New position (< curr_pos).
         * @param graph      Task graph.
         */
        inline void lmove(size_t curr_vid, size_t curr_pos, size_t target_pos, const Graph &graph);

        /**
         * Move a vertex right (toward the end) via forward rotation.
         * Adjusts release redistribution and helper pointers as needed.
         *
         * @param curr_vid   Vertex id being moved.
         * @param curr_pos   Current position.
         * @param target_pos New position (> curr_pos).
         * @param graph      Task graph.
         */
        inline void rmove(size_t curr_vid, size_t curr_pos, size_t target_pos, const Graph &graph);
    };
}
