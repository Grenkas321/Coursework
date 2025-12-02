#include "general_types.h"
#include "additionals.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Takes series-parallel graph and root of the tree that stores the series-parallel graph structure and constructs the optimal schedule for the given graph
     * \param cur_graph Series-parallel graph for which the schedule is constructed
     * \param root Pointer to the root of the tree that stores series-parallel graph structure
     * \return Pair <optimal schedule for the given graph, minmal weight topological cut for this graph> (second parameter is unused and needed because the function is recursive)
     */
    std::pair<ScheduleStatus, topological_cut> schedule_sp_graph(const Graph &cur_graph, Node *root);
    /**
     * Recomputes weights in the given graph after its transition from resources on edges to cumulative resources in nodes
     * \param g Graph that was modified  from resources on edges to cumulative resources in nodes
     * \param root Node of the given graph that does not have incoming edges and thus is considered a root node
     * \return Newly constructed graph
     */
    Graph recompute_weights(const Graph &g, int root);
    /**
     * Stores the number of each node as a property of the node
     * \param graph Graph for which node numbers are saved in nodes
     */
    void put_vertex_num(Graph &graph);
}
