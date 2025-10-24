#include "CorrectnessTester.h"
#include <iostream>

namespace scheduling_problem::algorithms
{
    bool test_correctness(const Graph &graph, const ScheduleStatus &schedule, bool &fl, Graph::edge_descriptor &bad_edge)
    {
        // check if number of tasks is cottect
        int n = boost::num_vertices(graph);

        std::vector<bool> visited_tasks(boost::num_vertices(graph) + 1);

        for (auto it : schedule)
        {
            if (visited_tasks[it.id])
            {
                return false;
            }

            visited_tasks[it.id] = 1;
        }

        // compute if all edges of graph are satisfied and comupute memory usage

        std::unordered_map<int, std::set<int>> computed_pred;

        for (auto it : schedule)
        {
            if (computed_pred[it.id].size() != boost::in_degree(it.id, graph))
            {
                if (fl)
                {
                    std::cout << "Tasks are executed in unavailable order. Task " << it.id << " at wrong place!\n"
                              << "Counted " << computed_pred[it.id].size() << " predecessors out of " << boost::in_degree(it.id, graph) << "\n";
                }

                return false;
            }

            for (auto out_e : boost::make_iterator_range(boost::out_edges(it.id, graph)))
            {
                computed_pred[out_e.m_target].insert(it.id);
            }
        }

        std::fill(visited_tasks.begin(), visited_tasks.end(), 0);

        for (auto it : schedule)
        {
            for (auto in_e : boost::make_iterator_range(boost::in_edges(it.id, graph)))
            {
                if (!visited_tasks[in_e.m_source])
                {
                    fl = 1;
                    bad_edge.m_source = in_e.m_source;
                    bad_edge.m_target = in_e.m_target;
                    return false;
                }
            }

            visited_tasks[it.id] = 1;
        }

        return true;
    }

    int peak_memory(const Graph &graph, const ScheduleStatus &schedule)
    {
        std::map<int, std::set<int>> computed_anc;
        int mem_usage = 0;
        int max_mem_usage = 0;

        for (auto it : schedule)
        {
            mem_usage += it.volume;

            if (mem_usage > max_mem_usage)
            {
                max_mem_usage = mem_usage;
            }

            if (!boost::out_degree(it.id, graph))
            {
                mem_usage -= it.volume;
            }

            for (auto in_e : boost::make_iterator_range(boost::in_edges(it.id, graph)))
            {
                computed_anc[in_e.m_source].insert(it.id);

                if (computed_anc[in_e.m_source].size() == boost::out_degree(in_e.m_source, graph))
                {
                    mem_usage -= boost::get(vertex_weight_t(), graph, in_e.m_source);
                }
            }
        }

        return max_mem_usage;
    }

    std::vector<int> memory_usage(const Graph &graph, ScheduleStatus &schedule)
    {
        std::map<int, std::set<int>> computed_anc;
        int mem_usage = 0;
        std::vector<int> usage;

        for (auto it : schedule)
        {
            mem_usage += boost::get(vertex_weight_t(), graph, it.id);

            usage.push_back(mem_usage);

            for (auto in_e : boost::make_iterator_range(boost::in_edges(it.id, graph)))
            {
                computed_anc[in_e.m_source].insert(it.id);

                if (computed_anc[in_e.m_source].size() == boost::out_degree(in_e.m_source, graph))
                {
                    mem_usage -= boost::get(vertex_weight_t(), graph, in_e.m_source);
                }
            }
        }

        return usage;
    }

    int peak_memory_on_edges(const Graph &graph, const ScheduleStatus &schedule)
    {
        int mem_usage = 0;
        int peak_mem_usage = 0;

        for (auto it : schedule)
        {
            for (auto out_e : boost::make_iterator_range(boost::out_edges(it.id, graph)))
            {
                mem_usage += boost::get(boost::edge_weight_t(), graph, out_e);
            }

            if (mem_usage > peak_mem_usage)
            {
                peak_mem_usage = mem_usage;
            }

            for (auto in_e : boost::make_iterator_range(boost::in_edges(it.id, graph)))
            {
                mem_usage -= boost::get(boost::edge_weight_t(), graph, in_e);
            }
        }

        return peak_mem_usage;
    }
}
