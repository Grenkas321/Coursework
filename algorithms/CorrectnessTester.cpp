#include "CorrectnessTester.h"
#include <iostream>

namespace scheduling_problem::algorithms
{
    /**
     * Check schedule correctness with respect to graph dependencies.
     *
     * Validates that:
     * 1) No job appears more than once in the schedule.
     * 2) Every task is executed only after all of its predecessors.
     *    If a violation is found and `fl` is true on entry, the function
     *    prints a diagnostic message and fills `bad_edge` with the offending edge.
     *
     * @param graph      Input task graph.
     * @param schedule   Schedule to validate.
     * @param fl         If true on entry, enables verbose diagnostics; also used as a flag on failure.
     * @param bad_edge   Output: the first edge that violates the topological order (set on failure).
     * @return           true if the schedule is correct; false otherwise.
     */
    bool test_correctness(const Graph &graph, const ScheduleStatus &schedule, bool &fl, Graph::edge_descriptor &bad_edge)
    {
        std::vector<bool> visited_tasks(boost::num_vertices(graph) + 1);

        for (auto it : schedule)
        {
            if (visited_tasks[it.id])
            {
                return false;
            }

            visited_tasks[it.id] = 1;
        }

        /**
         * Phase 1: verify that, at each task, all incoming edges come from already scheduled predecessors.
         * We count, for each vertex, how many predecessors have been seen so far.
         */
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

        /**
         * Phase 2: explicit edge-wise check and locate the first violating edge if any.
         */
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

    /**
     * Compute the peak memory usage along a schedule based on vertex weights.
     *
     * Model:
     * - When a task starts, its vertex weight is added to the current memory.
     * - If a task has no outgoing edges, its weight is immediately subtracted.
     * - For each predecessor u of the current task v, once all of u's outgoing
     *   edges have been "consumed" (all children scheduled), subtract weight(u).
     *
     * @param graph     Input task graph.
     * @param schedule  Schedule to evaluate.
     * @return          Maximum memory usage reached.
     */
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

    /**
     * Compute the memory usage trace after each scheduled task.
     *
     * The accumulation rule matches `peak_memory()`, but returns the running
     * memory after scheduling each job.
     *
     * @param graph     Input task graph.
     * @param schedule  Schedule to evaluate (non-const to match existing API; not modified).
     * @return          Vector of memory usage values after each step.
     */
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

    /**
     * Compute the peak memory usage measured on edges (edge weights).
     *
     * Model:
     * - For each task, add weights of all its outgoing edges.
     * - Track the peak of the accumulated sum.
     * - After the task, subtract weights of all its incoming edges.
     *
     * @param graph     Input task graph.
     * @param schedule  Schedule to evaluate.
     * @return          Maximum edge-based memory usage reached.
     */
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
