#include "general_types.h"
#include "ScheduleStatus.h"

#include "iostream"

namespace scheduling_problem::algorithms
{
    /**
     * Create new graph with the memory (resources) on its edges according to the given graph with the memory (resources) in its nodes
     * \param graph Input graph with the resources in nodes
     * \return Graph with the resources on edges that was created according to the input graph
     */
    Graph edge_to_vertex_mem(const Graph &graph);
    /**
     * Recompute the given tree for the series-parallel graph. Replace each node number with two node numbers with values 2 * N and 2 * N + 1. It is necessary, because each corresponding node in series-parallel graph was replaced with two nodes
     * \param root Root of the tree, that stores seres-parallel graph structure
     * \return Pointer to the new tree root that contains recomputed information about corresponding series-parallel graph
     */
    Node *RecomputeTree(Node *root);
    /**
     * Remove excess nodes from the schedule for the cumulative graph after the graph was transformed into graph with resources on edges
     * \param graph Graph with the resources on edges for which the output schedule is being generated
     * \param schedule %Schedule for the graph with cumulative resources in nodes, that is transformed to match the given graph
     * \return New schedule that contains two times less nodes and is correct for the given graph
     */
    ScheduleStatus clean_cum_weight_schedule(const Graph &graph, const ScheduleStatus &schedule);
}