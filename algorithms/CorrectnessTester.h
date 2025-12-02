#include "general_types.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Checks the current schedule for correctness on given graph
     * \param graph Graph that schedule must match
     * \param schedule Current schedule that is checked for correctness
     * \param fl Flag that enables logging information about graph
     * \param bad_edge If passed, this edge is checked first for correctness. Useful when schedule was changed slightly
     * \return True if schedule is correct, False otherwise
     */
    bool test_correctness(const Graph &graph, const ScheduleStatus &schedule, bool &fl, Graph::edge_descriptor &bad_edge);
    /**
     * Count the peak memory (resource) usage on the given graph with the memory (resource) in its nodes
     * \param graph Graph with the resource in its nodes
     * \param schedule %Schedule for the given graph, for which peak resource usage is counted
     * \return Peak resource usage on the given graph for the given schedule
     */
    int peak_memory(const Graph &graph, const ScheduleStatus &schedule);
    /**
     * Count the peak memory (resource) usage on the given graph with the memory (resource) on its edges
     * \param graph Graph with the resource on its edges
     * \param schedule %Schedule for the given graph, for which peak resource usage is counted
     * \return Peak resource usage on the given graph for the given schedule
     */
    int peak_memory_on_edges(const Graph &graph, const ScheduleStatus &schedule);
    /**
     * Count the memory (resource) usage on each step of the schedule for the given graph with the memory (resource) in its nodes
     * \param graph Graph with the resource in its nodes
     * \param schedule %Schedule for the given graph, for which peak resource usage is counted
     * \return Vector of resource usage on each step of schedule for the given graph
     */
    std::vector<int> memory_usage(const Graph &graph, ScheduleStatus &schedule);
}
