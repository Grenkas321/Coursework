#include "general_types.h"
#include "additionals.h"

namespace scheduling_problem
{
    /**
     * Topological sorting algorithm for the given graph
     * \param graph Graph for which topological sorting is performed
     * \return Vector of node numbers of the graph in topological order
     */
    std::vector<int> topo_sort(const Graph &graph);
}
