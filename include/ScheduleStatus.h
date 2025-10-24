#pragma once

#include "Schedule.h"

namespace scheduling_problem
{
    /**
     * @brief %Schedule representation with extended functionality
     *
     * ScheduleStatus is more flexible %Schedule representation. This class
     * allows to get tasks not only by their positions in the permutation (i.e. in the schedule)
     * but also using their node numbers. ScheduleStatus supports fast task insertion and
     * movement, checks if task is in the %schedule and gets its parents/children.
     */
    class ScheduleStatus : public Schedule
    {
        std::vector<size_t> positions_;
        std::vector<unsigned> child_remain_;
        std::vector<size_t *> release_on_;

        bool use_external_mem_;

    public:
        /**
         * @param graph Input graph
         * @param high_density True if graph has high density
         */
        ScheduleStatus(const Graph &graph, bool high_density = false);

        /**
         * @param graph Input graph
         * @param schedule %Schedule instance
         * @param high_density True if graph has high density
         */
        ScheduleStatus(const Graph &graph, const Schedule &schedule, bool high_density = false);

        /**
         * @param other Schedule to copy from
         */
        ScheduleStatus(const ScheduleStatus &other);

        /**
         * @param curr_vid %Node number in the graph
         * @return Position of the provided node
         */
        size_t loc(size_t curr_vid) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return The right bound (as a position in the schedule)
         */
        size_t upper(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return The left bound (as a position in the schedule)
         */
        size_t lower(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return Node numbers and positions where they release resources
         */
        std::pair<size_t, size_t> releaseOn(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @return True if the task is in the schedule, false otherwise
         */
        bool contains(size_t curr_vid) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return Numbers of parent nodes
         */
        std::vector<size_t> parents(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return Numbers of child nodes
         */
        std::vector<size_t> children(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         */
        boost::iterator_range<DiGraph::inv_adjacency_iterator>
        parentsf(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @return Task's children
         */
        boost::iterator_range<DiGraph::adjacency_iterator>
        childrenf(size_t curr_vid, const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param pos Position for insert
         * @param graph Input graph
         */
        void insert(size_t curr_vid, size_t pos, const Graph &graph);

        /**
         * @param curr_vid %Node number in the graph
         * @param tpos New position
         * @param graph Input graph
         * @return New schedule cost
         */
        weight_t move(size_t curr_vid, size_t tpos, const Graph &graph);

    private:
        /**
         * @param spos Start position
         * @param fpos Finish position
         */
        void updatePositions(size_t spos, size_t fpos);
        std::pair<bool, size_t> releasePosition(size_t curr_vid,
                                                const Graph &graph) const;

        /**
         * @param curr_vid %Node number in the graph
         * @param graph Input graph
         * @param target_pos New position
         */
        std::pair<bool, size_t> prevReleasePosition(size_t curr_vid,
                                                    const Graph &graph,
                                                    size_t target_pos) const;
        /**
         * @param curr_vid %Node number in the graph
         * @param curr_pos Current task position
         * @param target_pos New position
         * @param graph Input graph
         */
        void lmove(size_t curr_vid, size_t curr_pos, size_t target_pos, const Graph &graph);

        /**
         * @param curr_vid %Node number in the graph
         * @param curr_pos Current task position
         * @param target_pos New position
         * @param graph Input graph
         */
        void rmove(size_t curr_vid, size_t curr_pos, size_t target_pos, const Graph &graph);
    };
}