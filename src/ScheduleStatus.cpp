#include "ScheduleStatus.h"

namespace scheduling_problem
{
    /**
     * @brief Constructor
     *
     * For high density graphs, ScheduleStatus uses extra memory for caching
     * data, resulting in computation time decrease for
     * some operations
     */
    ScheduleStatus::ScheduleStatus(const Graph &graph,
                                   bool high_density)
        : Schedule(boost::num_vertices(graph), graph.name()), positions_(boost::num_vertices(graph), 0), use_external_mem_(high_density)
    {
        if (use_external_mem_)
        {
            auto size = boost::num_vertices(graph);
            child_remain_ = std::vector<unsigned>(size);
            for (const auto &curr_vid : boost::make_iterator_range(boost::vertices(graph)))
                child_remain_[curr_vid] = boost::out_degree(curr_vid, graph);
            release_on_ = std::vector<size_t *>(size, nullptr);
        }
    }

    /**
     * @brief Constructor
     */
    ScheduleStatus::ScheduleStatus(const Graph &graph,
                                   const Schedule &schedule,
                                   bool high_density)
        : ScheduleStatus(graph, high_density)
    {
        weight_t target(0);
        for (size_t pos(0); pos < schedule.size(); pos++)
        {
            auto curr_vid = schedule[pos].id;
            auto curr_weight(boost::get(vertex_weight_t(), graph, curr_vid));
            weight_t curr_release(boost::out_degree(curr_vid, graph) ? 0 : curr_weight);
            positions_[curr_vid] = pos;

            target += curr_weight;
            if (cost_ < target)
                cost_ = target;
            for (const auto &parent : parentsf(curr_vid, graph))
                if (releasePosition(parent, graph).first)
                    curr_release += boost::get(vertex_weight_t(), graph, parent);
            target -= curr_release;
            push_back(Job(curr_vid, curr_weight, curr_release));

            if (use_external_mem_)
            {
                for (const auto &parent : parentsf(curr_vid, graph))
                {
                    child_remain_[parent]--;
                    release_on_[parent] = &positions_[curr_vid];
                }
            }
        }
    }

    /**
     * @brief Constructor
     */
    ScheduleStatus::ScheduleStatus(const ScheduleStatus &other)
        : Schedule(other), positions_(other.positions_), child_remain_(other.child_remain_), use_external_mem_(other.use_external_mem_)
    {
        if (use_external_mem_)
        {
            release_on_ = std::vector<size_t *>(positions_.size(), nullptr);
            for (const auto &job : other)
            {
                auto release_pos_ptr = other.release_on_[job.id];
                if (release_pos_ptr)
                {
                    auto release_vid = other[*release_pos_ptr].id;
                    release_on_[job.id] = &positions_[release_vid];
                }
            }
        }
    }

    /**
     * @brief Get task by its node number
     */
    size_t ScheduleStatus::loc(size_t curr_vid) const
    {
        return positions_[curr_vid];
    }

    /**
     * @brief Get the right bound for task movement
     */
    size_t ScheduleStatus::upper(size_t curr_vid, const Graph &graph) const
    {
        // upper is excluded from movement segment
        if (!size())
            return 1;
        size_t upper_bound = size();
        for (const auto &child : children(curr_vid, graph))
            if (positions_[child] && positions_[child] < upper_bound)
                upper_bound = positions_[child];
        return upper_bound;
    }

    /**
     * @brief Get the left bound for task movement
     */
    size_t ScheduleStatus::lower(size_t curr_vid, const Graph &graph) const
    {
        // lower is included to movement segment
        size_t lower_bound = 0;
        for (const auto &parent : parents(curr_vid, graph))
            if (contains(parent) && positions_[parent] >= lower_bound)
                lower_bound = positions_[parent] + 1;
        return lower_bound;
    }

    /**
     * @brief Get the number of non-scheduled children and last child position
     */
    std::pair<size_t, size_t> ScheduleStatus::releaseOn(size_t curr_vid, const Graph &graph) const
    {
        size_t last_child(0), child_remain(boost::out_degree(curr_vid, graph));
        for (const auto &child : childrenf(curr_vid, graph))
            if (contains(child))
            {
                child_remain--;
                if (positions_[child] > last_child)
                    last_child = positions_[child];
            }
        return {child_remain, last_child};
    }

    /**
     * @brief Get task parents (excluding imaginary edges)
     */
    std::vector<size_t> ScheduleStatus::parents(size_t curr_vid, const Graph &graph) const
    {
        auto p = boost::make_iterator_range(boost::inv_adjacent_vertices(curr_vid, graph));
        std::vector<size_t> fp;
        for (const auto &vid : p)
        {
            auto type = static_cast<const EdgeProperties *>(boost::edge(vid, curr_vid, graph).first.get_property())->m_base.m_value;
            if (type == EdgeKind::Real)
                fp.push_back(vid);
        }
        return fp;
    }

    /**
     * @brief Get task children (excluding imaginary edges)
     */
    std::vector<size_t> ScheduleStatus::children(size_t curr_vid, const Graph &graph) const
    {
        auto c = boost::make_iterator_range(boost::adjacent_vertices(curr_vid, graph));
        std::vector<size_t> fc;
        for (const auto &vid : c)
        {
            auto type = static_cast<const EdgeProperties *>(boost::edge(curr_vid, vid, graph).first.get_property())->m_base.m_value;
            if (type == EdgeKind::Real)
                fc.push_back(vid);
        }
        return fc;
    }

    /**
     * @brief Get task parents (including imaginary edges)
     */
    boost::iterator_range<DiGraph::inv_adjacency_iterator>
    ScheduleStatus::parentsf(size_t curr_vid, const Graph &graph) const
    {
        return boost::make_iterator_range(boost::inv_adjacent_vertices(curr_vid, graph));
    }

    /**
     * @brief Get task children (including imaginary edges)
     */
    boost::iterator_range<DiGraph::adjacency_iterator>
    ScheduleStatus::childrenf(size_t curr_vid, const Graph &graph) const
    {
        return boost::make_iterator_range(boost::adjacent_vertices(curr_vid, graph));
    }

    /**
     * @brief Find out if the schedule contains the task
     */
    bool ScheduleStatus::contains(size_t curr_vid) const
    {
        return curr_vid < positions_.size() && (positions_[curr_vid] || (size() && (operator[](0).id == curr_vid)));
    }

    /**
     * @brief Inserts the task to the schedule
     */
    void ScheduleStatus::insert(size_t curr_vid, size_t pos, const Graph &graph)
    {
        auto weights = boost::get(vertex_weight_t(), graph);
        size_t curr_release = boost::out_degree(curr_vid, graph) ? 0 : weights[curr_vid];
        std::vector<Job>::insert(begin() + pos, Job(curr_vid, weights[curr_vid], curr_release));
        for (size_t curr_pos(pos); curr_pos < size(); curr_pos++)
            positions_[operator[](curr_pos).id] = curr_pos;

        if (use_external_mem_)
        {
            for (const auto &parent : parentsf(curr_vid, graph))
            {
                child_remain_[parent]--;
                if (!release_on_[parent] || pos > *release_on_[parent])
                    release_on_[parent] = &positions_[curr_vid];
                if (!child_remain_[parent])
                    operator[](*release_on_[parent]).release += weights[parent];
            }
        }
        else
        {
            size_t release_pos;
            bool is_released;
            for (const auto &parent : parentsf(curr_vid, graph))
            {
                std::tie(is_released, release_pos) = releasePosition(parent, graph);
                if (is_released)
                    operator[](release_pos).release += weights[parent];
            }
        }

        cost(true); // recompute cost
    }

    /**
     * @brief Moves the task to another position
     */
    weight_t ScheduleStatus::move(size_t curr_vid, size_t tpos, const Graph &graph)
    {
        bool recompute(true);
        size_t curr_pos(positions_[curr_vid]);
        if (curr_pos < tpos)
            rmove(curr_vid, curr_pos, tpos, graph);
        else if (curr_pos > tpos)
            lmove(curr_vid, curr_pos, tpos, graph);
        else
            recompute = false;

        return cost(recompute); // recompute cost
    }

    /**
     * @brief Updates positions information for task from range [spos, fpos]
     */
    inline void ScheduleStatus::updatePositions(size_t spos, size_t fpos)
    {
        for (auto pos(spos); pos <= fpos; pos++)
            positions_[operator[](pos).id] = pos;
    }

    /**
     * @brief Get the position in which task's resources are released
     */
    inline std::pair<bool, size_t> ScheduleStatus::releasePosition(size_t curr_vid,
                                                                   const Graph &graph) const
    {
        size_t release_pos(0);
        for (const auto &child : childrenf(curr_vid, graph))
        {
            if (!contains(child)) // positions_[child] <= positions_[curr_vid]
                return {false, 0};
            if (positions_[child] > release_pos)
                release_pos = positions_[child];
        }
        return {true, release_pos};
    }

    /**
     * @brief Get the previous position in which task's resources were released
     */
    inline std::pair<bool, size_t> ScheduleStatus::prevReleasePosition(size_t curr_vid,
                                                                       const Graph &graph,
                                                                       size_t target_pos) const
    {
        size_t prev_release_pos(0);
        for (const auto &child : childrenf(curr_vid, graph))
        {
            if (!contains(child))
                return {false, 0};
            if (positions_[child] != target_pos && prev_release_pos < positions_[child])
                prev_release_pos = positions_[child];
        }
        return {true, prev_release_pos};
    }

    /**
     * @brief Moves the task to another position in the left schedule part
     */
    inline void ScheduleStatus::lmove(size_t curr_vid,
                                      size_t curr_pos,
                                      size_t target_pos,
                                      const Graph &graph)
    {
        size_t rcurr_pos(size() - curr_pos - 1);
        size_t rtarg_pos(size() - target_pos - 1);
        std::rotate(rbegin() + rcurr_pos, rbegin() + rcurr_pos + 1, rbegin() + rtarg_pos + 1); // cycle shifting
        updatePositions(target_pos, curr_pos);

        bool is_released;
        size_t release_pos;
        for (const auto &parent : parentsf(curr_vid, graph))
        {
            std::tie(is_released, release_pos) = releasePosition(parent, graph);
            if (is_released)
            {
                if (release_pos > target_pos && release_pos <= curr_pos)
                {
                    auto pweight(boost::get(vertex_weight_t(), graph, parent));
                    operator[](target_pos).release -= pweight;
                    operator[](release_pos).release += pweight;

                    if (use_external_mem_)
                        release_on_[parent] = &positions_[operator[](release_pos).id];
                }
            }
        }
    }

    /**
     * @brief Moves the task to another position in the right schedule part
     */
    inline void ScheduleStatus::rmove(size_t curr_vid,
                                      size_t curr_pos,
                                      size_t target_pos,
                                      const Graph &graph)
    {
        std::rotate(begin() + curr_pos, begin() + curr_pos + 1, begin() + target_pos + 1);
        updatePositions(curr_pos, target_pos);

        if (use_external_mem_)
        {
            for (const auto &parent : parentsf(curr_vid, graph))
                if (target_pos > *release_on_[parent])
                {
                    if (!child_remain_[parent])
                    {
                        auto pweight(boost::get(vertex_weight_t(), graph, parent));
                        operator[](*release_on_[parent]).release -= pweight;
                        operator[](target_pos).release += pweight;
                    }
                    release_on_[parent] = &positions_[operator[](target_pos).id];
                }
        }
        else
        {
            bool is_released;
            size_t prev_release_pos;
            for (const auto &parent : parentsf(curr_vid, graph))
            {
                std::tie(is_released, prev_release_pos) = prevReleasePosition(parent, graph, target_pos);
                if (is_released)
                {
                    if (prev_release_pos < target_pos && prev_release_pos >= curr_pos)
                    {
                        auto pweight(boost::get(vertex_weight_t(), graph, parent));
                        operator[](prev_release_pos).release -= pweight;
                        operator[](target_pos).release += pweight;
                    }
                }
            }
        }
    }
}
