#include "GreedyHeuristics.h"

namespace scheduling_problem::algorithms
{
    /**
     * Reverse DFS: traverse incoming edges to find all vertices
     * that can reach v (i.e., DFS on the reversed graph).
     *
     * @param g        Input graph.
     * @param visited  Set updated with visited vertex ids.
     * @param v        Start vertex.
     */
    void rev_dfs_(const Graph &g, std::unordered_set<int> &visited, int v)
    {
        visited.insert(v);

        for (auto it : boost::make_iterator_range(boost::in_edges(v, g)))
        {
            if (visited.find(it.m_source) == visited.end())
            {
                rev_dfs_(g, visited, it.m_source);
            }
        }
    }

    /**
     * Insert the edge (source -> target) by minimally reordering the schedule.
     *
     * If source already precedes target, returns a copy of the input schedule.
     * Otherwise, tries to move the pair (source, target) closer together by
     * keeping predecessors of source in place and inserting a limited number of
     * intermediate nodes between them. Chooses the variant with the smallest
     * objective value (schedule.cost()).
     *
     * @param graph    Input graph.
     * @param schedule Current schedule.
     * @param source   Edge source vertex.
     * @param target   Edge target vertex.
     * @return         Updated schedule with the edge respected.
     */
    ScheduleStatus add_edge_simple(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor source, Graph::vertex_descriptor target)
    {
        auto source_pos = schedule.loc(source);
        auto target_pos = schedule.loc(target);

        if (source_pos <= target_pos)
        {
            auto tmp = schedule;
            return tmp;
        }

        ScheduleStatus cur_schedule(graph);
        ScheduleStatus best_schedule(graph);
        int best_tf = INT_MAX;

        std::unordered_set<int> visited;
        rev_dfs_(graph, visited, source);

        for (auto node : visited)
        {
            if (schedule.loc(node) < target)
            {
                visited.erase(node);
            }
        }

        for (size_t k = 0; k < source_pos - target_pos - visited.size(); ++k)
        {
            cur_schedule.clear();
            // prefix up to target_pos
            for (size_t i = 0; i < target_pos; ++i)
            {
                cur_schedule.insert(schedule[i].id, cur_schedule.size(), graph);
            }
            // predecessors of source between [target_pos, source_pos)
            for (size_t i = target_pos; i < source_pos; ++i)
            {
                if (visited.find(schedule[i].id) != visited.end())
                {
                    cur_schedule.insert(schedule[i].id, cur_schedule.size(), graph);
                }
            }
            // insert k non-predecessor nodes before the (source, target) pair
            for (size_t i = 0; i < k; ++i)
            {
                int pos = target_pos + 1 + i;
                if (visited.find(schedule[pos].id) == visited.end())
                {
                    cur_schedule.insert(schedule[pos].id, cur_schedule.size(), graph);
                }
            }
            // place source then target
            cur_schedule.insert(schedule[source_pos].id, cur_schedule.size(), graph);
            cur_schedule.insert(schedule[target_pos].id, cur_schedule.size(), graph);
            // remaining non-predecessor nodes between target and source
            for (size_t i = k; i < source_pos - target_pos - visited.size(); ++i)
            {
                int pos = target_pos + 1 + i;
                if (visited.find(schedule[pos].id) == visited.end())
                {
                    cur_schedule.insert(schedule[pos].id, cur_schedule.size(), graph);
                }
            }
            // suffix after source
            for (size_t i = source_pos + 1; i < schedule.size(); ++i)
            {
                cur_schedule.insert(schedule[i].id, cur_schedule.size(), graph);
            }

            int cur_tf = cur_schedule.cost();
            if (cur_tf < best_tf)
            {
                best_schedule = cur_schedule;
                best_tf = cur_tf;
            }
        }

        return best_schedule;
    }

    /**
     * Pick the next vertex (from a set of not-yet-inserted vertices) that
     * maximizes the number of incident edges already “covered” by the current schedule.
     *
     * Coverage counts how many in-edges come from scheduled parents and how many
     * out-edges go to scheduled children. Ties are broken by the latest candidate seen.
     *
     * @param graph     Input graph.
     * @param schedule  Current schedule.
     * @param vertices  Set of candidate vertices not yet inserted.
     * @return          Chosen vertex id.
     */
    int next_vertex(const Graph &graph, const ScheduleStatus &schedule, std::unordered_set<int> &vertices)
    {
        int best_v = 0;
        int best_edge_num = 0;

        for (auto v : vertices)
        {
            int cur_edge_num = 0;
            for (auto it : boost::make_iterator_range(boost::in_edges(v, graph)))
            {
                if (schedule.contains(it.m_source))
                {
                    cur_edge_num++;
                }
            }
            for (auto it : boost::make_iterator_range(boost::out_edges(v, graph)))
            {
                if (schedule.contains(it.m_target))
                {
                    cur_edge_num++;
                }
            }

            if (cur_edge_num >= best_edge_num)
            {
                best_edge_num = cur_edge_num;
                best_v = v;
            }
        }

        return best_v;
    }
}
