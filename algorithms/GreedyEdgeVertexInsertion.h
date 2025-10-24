#include "general_types.h"
#include "additionals.h"
#include "CorrectnessTester.h"
#include "ScheduleStatus.h"

#include "GreedyHeuristics.h"

#include "boost/graph/depth_first_search.hpp"

namespace scheduling_problem::algorithms
{
    /**
     * %Greedy %heuristics for insertion of nodes and edges into the given subgraph so that it will be equal to the given graph. It also modifies the given schedule for the subgraph so that it will match the graph
     * \param graph Full graph that contains all nodes and edges
     * \param sp_graph Subgraph of the graph that we modify by adding edges and nodes
     * \param schedule %Schedule for the sp_graph that is modified to match the given graph
     * \return New schedule that is correct for the given graph
     */
    ScheduleStatus insert_vertices_and_edges(const Graph &graph, Graph sp_graph, const ScheduleStatus &schedule);
    /**
     * %Greedy heuristics to add edge of the given graph into schedule. And modify the schedule if needed, to guarantee its correctness
     * \param graph Graph in which the edge is added
     * \param schedule %Schedule that is modified after edge addition into graph
     * \param source Source node of the inserted edge
     * \param target Target node of the inserted edge
     * \return New schedule that is correct after edge insertion
     */
    ScheduleStatus add_edge(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor source, Graph::vertex_descriptor target);
}