#include "FindSeriesParallel.h"

#include <unordered_map>
#include <vector>
#include <unordered_set>
#include <map>
#include <set>

namespace scheduling_problem::algorithms
{
    /**
     * Mapping from a reduced edge (u,v) to the set of original edges
     * subsumed by it during series/parallel reductions.
     */
    std::map<std::pair<int, int>, std::set<std::pair<int, int>>> initial_vert;

    /**
     * Series/parallel decomposition tree:
     * for each current edge (u,v) stores a Node* describing the substructure
     * merged into this edge (with state 'S' or 'P').
     */
    std::map<std::pair<int, int>, Node *> Tree;

    /**
     * Remove edges that participate in series or parallel combinations.
     *
     * While there exists a vertex with in-degree=1 and out-degree=1:
     *   - Let parent -> v -> child be the chain.
     *   - Merge the two edges into (parent, child) (series node).
     *   - If (parent, child) already exists, merge as parallel.
     *   - Update edge weights and the decomposition Tree/initial_vert maps.
     *
     * @param graph  Graph to reduce in-place.
     */
    void delete_series_parallel_edges(Graph &graph)
    {
        bool removed_edge = true;

        while (removed_edge)
        {
            removed_edge = false;

            for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
            {
                if (boost::in_degree(vertex, graph) == 1 && boost::out_degree(vertex, graph) == 1)
                {
                    removed_edge = true;

                    auto in_edge = boost::make_iterator_range(boost::in_edges(vertex, graph))[0];
                    auto out_edge = boost::make_iterator_range(boost::out_edges(vertex, graph))[0];
                    auto parent_vert = boost::source(*boost::in_edges(vertex, graph).first, graph);
                    auto son_vert = boost::target(*boost::out_edges(vertex, graph).first, graph);

                    std::unordered_set<int> merged_vert = Tree[{parent_vert, vertex}]->vertices;
                    merged_vert.insert(Tree[{vertex, son_vert}]->vertices.begin(), Tree[{vertex, son_vert}]->vertices.end());
                    auto new_series_node = new Node(merged_vert, Tree[{parent_vert, vertex}], Tree[{vertex, son_vert}], 'S');

                    Tree.erase({parent_vert, vertex});
                    Tree.erase({vertex, son_vert});

                    auto in_w = boost::get(boost::edge_weight_t(), graph, in_edge);
                    auto out_w = boost::get(boost::edge_weight_t(), graph, out_edge);

                    boost::remove_edge(parent_vert, vertex, graph);
                    boost::remove_edge(vertex, son_vert, graph);

                    std::pair<int, int> new_edge = {parent_vert, son_vert};

                    if (initial_vert.find(new_edge) == initial_vert.end())
                    {
                        auto new_e = boost::add_edge(parent_vert, son_vert, graph);
                        boost::put(boost::edge_weight_t(), graph, new_e.first, in_w + out_w);

                        Tree[{parent_vert, son_vert}] = new_series_node;
                    }
                    else
                    {
                        merged_vert = new_series_node->vertices;
                        merged_vert.insert(Tree[{parent_vert, son_vert}]->vertices.begin(), Tree[{parent_vert, son_vert}]->vertices.end());
                        auto new_parallel_node = new Node(merged_vert, Tree[{parent_vert, son_vert}], new_series_node, 'P');

                        Tree[{parent_vert, son_vert}] = new_parallel_node;

                        auto e = boost::edge(parent_vert, son_vert, graph).first;
                        auto tmp = boost::get(boost::edge_weight_t(), graph, e);
                        boost::put(boost::edge_weight_t(), graph, e, tmp + in_w + out_w);
                    }

                    initial_vert[new_edge].insert(initial_vert[{parent_vert, vertex}].begin(), initial_vert[{parent_vert, vertex}].end());
                    initial_vert[new_edge].insert(initial_vert[{vertex, son_vert}].begin(), initial_vert[{vertex, son_vert}].end());

                    initial_vert.erase({parent_vert, vertex});
                    initial_vert.erase({vertex, son_vert});

                    break;
                }
            }
        }
    }

    /**
     * Compute immediate dominators for a directed graph using
     * the Lengauer-Tarjan algorithm.
     *
     * The root is chosen as a vertex with in-degree 0 and out-degree > 0
     * (or the first vertex if none found). Returns an array `idom` where
     * idom[index(v)] = index(u) if u immediately dominates v, or max<int>
     * for the root.
     *
     * @tparam Graph  Boost-compatible directed graph type.
     * @param graph   Graph to analyze.
     * @return        Vector of immediate dominator indices.
     */
    template <class Graph>
    std::vector<int> find_dom_tree(Graph &graph)
    {
        auto root = boost::make_iterator_range(boost::vertices(graph))[0];

        for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (boost::in_degree(vertex, graph) == 0 && boost::out_degree(vertex, graph) != 0)
            {
                root = vertex;
                break;
            }
        }

        std::vector<Vertex> domTreePredVector;
        IndexMap indexMap(boost::get(boost::vertex_index, graph));
        typename boost::graph_traits<Graph>::vertex_iterator uItr, uEnd;

        domTreePredVector = std::vector<Vertex>(boost::num_vertices(graph), boost::graph_traits<Graph>::null_vertex());
        PredMap domTreePredMap = boost::make_iterator_property_map(domTreePredVector.begin(), indexMap);

        boost::lengauer_tarjan_dominator_tree(graph, root, domTreePredMap);

        std::vector<int> idom(boost::num_vertices(graph));
        for (boost::tie(uItr, uEnd) = boost::vertices(graph); uItr != uEnd; ++uItr)
        {
            if (boost::get(domTreePredMap, *uItr) != boost::graph_traits<Graph>::null_vertex())
                idom[boost::get(indexMap, *uItr)] = boost::get(indexMap, boost::get(domTreePredMap, *uItr));
            else
                idom[boost::get(indexMap, *uItr)] = (std::numeric_limits<int>::max)();
        }

        return idom;
    }

    /**
     * Remove one edge that prevents the graph from being series-parallel.
     *
     * Uses dominator trees of the graph and its reverse. An edge (u,v) is a
     * candidate for removal if:
     *  - idom(v) != u and idom_rev(u) != v (i.e., not part of a dominator chain),
     *  - u != st_vertex and v != end_vertex (do not remove boundary edges).
     * Among candidates, the edge with the smallest weight is removed.
     *
     * @param graph       Graph to modify in-place.
     * @param st_vertex   Start vertex to protect (or -1 if none).
     * @param end_vertex  End vertex to protect (or -1 if none).
     */
    void delete_excess_edge(Graph &graph, int st_vertex, int end_vertex)
    {
        auto idom = find_dom_tree(graph);
        auto rev_graph = boost::make_reverse_graph(graph);
        auto rev_idom = find_dom_tree(rev_graph);

        Graph::edge_descriptor edge_to_remove;
        int min_edge_w = INT_MAX;

        for (auto edge : boost::make_iterator_range(boost::edges(graph)))
        {
            if (idom[edge.m_target] != static_cast<int>(edge.m_source) &&
                        rev_idom[edge.m_source] != static_cast<int>(edge.m_target) &&
                        static_cast<size_t>(edge.m_source) != static_cast<size_t>(st_vertex) &&
                        static_cast<size_t>(edge.m_target) != static_cast<size_t>(end_vertex))
            {
                auto cur_w = boost::get(boost::edge_weight_t(), graph, edge);
                if (cur_w < min_edge_w)
                {
                    min_edge_w = cur_w;
                    edge_to_remove = edge;
                }
            }
        }

        if (min_edge_w != INT_MAX)
        {
            boost::remove_edge(edge_to_remove, graph);
            initial_vert.erase({edge_to_remove.m_source, edge_to_remove.m_target});
        }
    }

    /**
     * Extract the largest series-parallel subgraph and its SP-tree.
     *
     * Procedure:
     * 1) Record original edge and vertex weights.
     * 2) Initialize `initial_vert` and `Tree` with all edges.
     * 3) Repeatedly:
     *      - contract series/parallel patterns,
     *      - if needed, remove an excess edge by dominator analysis,
     *    until no progress or a single edge remains.
     * 4) Reconstruct the resulting SP subgraph with original weights.
     *
     * @param graph  Input graph (copied by value; original is unchanged).
     * @return       Pair {SP-subgraph, root of SP-tree}.
     */
    std::pair<Graph, Node *> find_sp_subgraph(Graph graph)
    {
        int st_vertex = -1, end_vertex = -1;
        bool have_vertices = false;

        std::map<std::pair<int, int>, int> edge_weights;

        for (auto it : boost::make_iterator_range(boost::edges(graph)))
        {
            edge_weights[{it.m_source, it.m_target}] = boost::get(boost::edge_weight_t(), graph, it);
        }

        std::unordered_map<int, int> vertex_weights;

        for (auto it : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (!boost::in_degree(it, graph))
            {
                st_vertex = it;
                have_vertices = true;
            }
            if (!boost::out_degree(it, graph))
            {
                end_vertex = it;
                have_vertices = true;
            }

            vertex_weights[it] = boost::get(vertex_weight_t(), graph, it);
        }

        for (auto edge : boost::make_iterator_range(boost::edges(graph)))
        {
            initial_vert[{edge.m_source, edge.m_target}] = {{edge.m_source, edge.m_target}};

            std::unordered_set<int> s = {(int)edge.m_source, (int)edge.m_target};
            Node *new_edge = new Node(s);

            Tree[{edge.m_source, edge.m_target}] = new_edge;
        }

        while (boost::num_edges(graph) > 1)
        {
            int tmp = boost::num_edges(graph);
            delete_series_parallel_edges(graph);
            int d1 = tmp - boost::num_edges(graph);
            int d2 = 0;

            if (boost::num_edges(graph) > 1)
            {
                tmp = boost::num_edges(graph);
                if (have_vertices)
                {
                    delete_excess_edge(graph, st_vertex, end_vertex);
                }
                d2 = tmp - boost::num_edges(graph);
            }

            if (!d1 && !d2)
            {
                break;
            }
        }

        auto sp_edges = (*initial_vert.begin()).second;

        auto root = (*Tree.begin()).second;

        Graph sp_graph;

        for (auto edge : sp_edges)
        {
            auto new_edge = boost::add_edge(edge.first, edge.second, sp_graph);
            auto edge_weight = edge_weights[{edge.first, edge.second}];
            auto source_weight = vertex_weights[new_edge.first.m_source];
            auto target_weight = vertex_weights[new_edge.first.m_target];

            boost::put(boost::edge_weight_t(), sp_graph, new_edge.first, edge_weight);
            boost::put(vertex_weight_t(), sp_graph, new_edge.first.m_source, source_weight);
            boost::put(vertex_weight_t(), sp_graph, new_edge.first.m_target, target_weight);
        }

        Tree.clear();
        initial_vert.clear();

        return {sp_graph, root};
    }

    /**
     * Recursively delete an SP-tree.
     *
     * @param root  Tree root to delete (may be null).
     */
    void delete_tree(Node *root)
    {
        if (!root)
        {
            return;
        }

        if (root->left)
        {
            delete_tree(root->left);
        }
        if (root->right)
        {
            delete_tree(root->right);
        }

        delete root;
    }
}
