#include "general_types.h"
#include "additionals.h"
#include "CorrectnessTester.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * %Greedy heuristics to add edge of the given graph into schedule. And modify the schedule if needed, to guarantee its correctness
     * \param graph Graph in which the edge is added
     * \param schedule %Schedule that is modified after edge addition into graph
     * \param source Source node of the inserted edge
     * \param target Target node of the inserted edge
     * \return New schedule that is correct after edge insertion
     */
    ScheduleStatus add_edge_simple(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor source, Graph::vertex_descriptor target);
}
