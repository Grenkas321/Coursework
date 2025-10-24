#include "GreedyHeuristics.h"

namespace scheduling_problem::algorithms
{
    /**
     * Depth first search on the given graph and start node v, considering all edges in it are reversed
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

    ScheduleStatus add_edge_simple(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor source, Graph::vertex_descriptor target)
    {
        auto source_pos = schedule.loc(source);
        auto target_pos = schedule.loc(target);

        if (source_pos <= target_pos)
        {
            // all requirements already satisfied
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

        for (int k = 0; k < source_pos - target_pos - visited.size(); ++k)
        {
            cur_schedule.clear();
            // insert prefix
            for (int i = 0; i < target_pos; ++i)
            {
                cur_schedule.insert(schedule[i].id, cur_schedule.size(), graph);
            }
            // insert source predecessors
            for (int i = target_pos; i < source_pos; ++i)
            {
                if (visited.find(schedule[i].id) != visited.end())
                {
                    cur_schedule.insert(schedule[i].id, cur_schedule.size(), graph);
                }
            }
            // insert k nodes before (source, target) pair
            for (int i = 0; i < k; ++i)
            {
                int pos = target_pos + 1 + i;
                if (visited.find(schedule[pos].id) == visited.end())
                {
                    cur_schedule.insert(schedule[pos].id, cur_schedule.size(), graph);
                }
            }
            // insert source and target
            cur_schedule.insert(schedule[source_pos].id, cur_schedule.size(), graph);
            cur_schedule.insert(schedule[target_pos].id, cur_schedule.size(), graph);
            // insert remaining nodes from between source and target
            for (int i = k; i < source_pos - target_pos - visited.size(); ++i)
            {
                int pos = target_pos + 1 + i;
                if (visited.find(schedule[pos].id) == visited.end())
                {
                    cur_schedule.insert(schedule[pos].id, cur_schedule.size(), graph);
                }
            }
            // insert suffix nodes that were after source
            for (int i = source_pos + 1; i < schedule.size(); ++i)
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
     * Determine which of the nodes that were not added to the subgraph must be added next to the subgraph and to the corresponding schedule
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