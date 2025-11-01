#include "ToOneSourceTarget.h"

namespace scheduling_problem::algorithms
{
    Graph add_source_target(const Graph &graph)
    {
        // function to merge all source/target nodes into one node

        auto new_graph = graph;
        auto source = boost::add_vertex(new_graph);
        auto target = boost::add_vertex(new_graph);

        for (auto it : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (boost::in_degree(it, graph) == 0)
            {
                // its a source node
                boost::add_edge(source, it, new_graph);
            }

            if (boost::out_degree(it, graph) == 0)
            {
                // its a target node
                boost::add_edge(it, target, new_graph);
            }
        }

        return new_graph;
    }
}
