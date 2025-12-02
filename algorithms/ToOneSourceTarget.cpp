#include "ToOneSourceTarget.h"

namespace scheduling_problem::algorithms
{
    /**
 * @brief Add a single super-source and super-target to a DAG.
 *
 * Creates two new vertices: a super-source with edges to all original source
 * vertices (in-degree == 0) and a super-target with edges from all original
 * sink vertices (out-degree == 0). Vertex/edge weights of the original graph
 * remain unchanged; only two vertices and the connecting edges are added.
 *
 * @param graph  Input DAG.
 * @return A copy of @p graph augmented with the super-source and super-target.
 */
    Graph add_source_target(const Graph &graph)
    {

        auto new_graph = graph;
        auto source = boost::add_vertex(new_graph);
        auto target = boost::add_vertex(new_graph);

        for (auto it : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (boost::in_degree(it, graph) == 0)
            {
                boost::add_edge(source, it, new_graph);
            }

            if (boost::out_degree(it, graph) == 0)
            {
                boost::add_edge(it, target, new_graph);
            }
        }

        return new_graph;
    }
}
