#include "general_types.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Takes graph with the resources in nodes and creates new graph that corresponds to the given graph but has resources on edges
     * \param graph Graph with resources in nodes for which new graph is generated
     * \return New graph with the resources on edges that corresponds to the given graph
     */
    Graph to_edge_memory(const Graph &graph);
    /**
     * Remove excess nodes from the schedule for the given graph that was transfered from statement with resources on edges back to statement with resources in nodes
     * \param graph Graph with the resources in nodes
     * \param schedule %Schedule that is modified to match the graph
     * \return New schedule that is correct for graph and was constructed from the initial schedule
     */
    ScheduleStatus clean_edge_weight_schedule(const Graph &graph, const ScheduleStatus &schedule);
}