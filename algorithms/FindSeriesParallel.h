#include "general_types.h"
#include "ToEdgeMemory.h"

#include <iostream>

namespace scheduling_problem::algorithms
{
    /**
     * Define edge iterator for the choosen Boost::Graph
     */
    typedef boost::detail::edge_desc_impl<boost::bidirectional_tag, std::size_t> edge_iterator;

    /**
     * Find series-parallel subgraph in the given graph
     * \param graph The graph that is used to find series-parallel subgraph
     * \return Pair <series-parallel subgraph, root of the tree that stores structure for this series-parallel graph>
     */
    std::pair<Graph, Node *> find_sp_subgraph(Graph graph);

    /**
     * Free up memory after the tree that stores series-parallel graph structure is no more needed
     * \param root Root of the tree that is being freed
     */
    void delete_tree(Node *root);
}
