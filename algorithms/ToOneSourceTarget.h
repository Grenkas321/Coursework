#include "general_types.h"

namespace scheduling_problem::algorithms
{
    /**
     * Finds all nodes with zero incoming edge degree and adds one source node, that is a parent for all such nodes in graph. Same with nodes with zero outgoing edges degree, one final node is added as their common child
     * \param graph Graph that is being modified
     * \return New graph with exactly one input and output node
     */
    Graph add_source_target(const Graph &graph);
}
