#pragma once

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>
#include <string>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/subgraph.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/property_map/property_map.hpp>

/**
 * Implemented data structures and types for task-graph scheduling.
 * Provides a directed graph model with vertex/edge properties and helpers.
 */
namespace scheduling_problem
{
    /**
     * Vertex property tag: per-vertex weight (e.g., produced volume).
     * Use with boost::get(vertex_weight_t(), graph, v).
     */
    struct vertex_weight_t
    {
        typedef boost::vertex_property_tag kind;
    };

    /**
     * Vertex property tag: an auxiliary numeric label for a vertex.
     * Use with boost::get(vertex_num_t(), graph, v).
     */
    struct vertex_num_t
    {
        typedef boost::vertex_property_tag kind;
    };

    /**
     * Scalar type for weights (vertex weights, edge weights, buffer sizes).
     */
    typedef long long weight_t;

    /**
     * Scalar type for auxiliary numeric labels.
     */
    typedef std::size_t num_t;

    /**
     * Composite vertex property:
     * - vertex_weight_t -> weight_t
     * - vertex_num_t    -> num_t
     * - vertex_index_t  -> std::size_t (required by Boost.Graph)
     */
    typedef boost::property<vertex_weight_t, weight_t,
                            boost::property<vertex_num_t, num_t,
                                            boost::property<boost::vertex_index_t, std::size_t>>>
        VertexProperty;

    /**
     * Edge property tag: semantic kind of an edge (Real/Imaginary).
     * Use with boost::get(edge_kind_t(), graph, e).
     */
    struct edge_kind_t
    {
        typedef boost::edge_property_tag kind;
    };

    /**
     * Edge property tag: buffer identifier associated with an edge.
     * Use with boost::get(edge_buffer_id_t(), graph, e).
     */
    struct edge_buffer_id_t
    {
        typedef boost::edge_property_tag kind;
    };

    /**
     * Edge kind semantics:
     * - Real:     an original dependency in the input graph.
     * - Imaginary:an auxiliary edge introduced to aid scheduling.
     */
    enum class EdgeKind
    {
        Real      = false,
        Imaginary = true
    };

    /**
     * Edge property bundle for (kind + index).
     * Note: edge_index_t is included for Boost.Graph algorithms that require it.
     */
    typedef boost::property<edge_kind_t,
                            EdgeKind,
                            boost::property<boost::edge_index_t, size_t>>
        EdgeKindProperty;

    /**
     * Full edge property bundle:
     * - edge_buffer_id_t -> int
     * - edge_weight_t    -> weight_t (buffer size / release amount)
     * - EdgeKindProperty -> kind + index
     */
    typedef boost::property<edge_buffer_id_t,
                            int,
                            boost::property<boost::edge_weight_t,
                                            weight_t,
                                            EdgeKindProperty>>
        EdgeProperties;

    /**
     * Directed graph type used throughout the scheduler.
     * Vertex storage: vecS, Edge storage: vecS, Directed: bidirectionalS.
     * Vertex properties: VertexProperty, Edge properties: EdgeProperties.
     */
    typedef boost::adjacency_list<boost::vecS,
                                  boost::vecS,
                                  boost::bidirectionalS,
                                  VertexProperty,
                                  EdgeProperties>
        DiGraph;

    /**
     * Directed acyclic graph with a human-readable name and helpers.
     * Inherits from DiGraph and adds:
     * - a stored name string,
     * - a helper for adding imaginary edges with explicit properties.
     */
    class Graph : public DiGraph
    {
    private:
        /** Human-readable graph name (used in serialization and logs). */
        std::string name_;

    public:
        /**
         * Construct an empty graph with a given number of vertices and name.
         *
         * @param n_vertex  Initial vertex count (default: 0).
         * @param name      Graph name (default: "dag").
         */
        Graph(size_t n_vertex = 0,
              const std::string &name = "dag")
            : DiGraph(n_vertex), name_(name)
        {
        }

        /**
         * Copy-construct from another Graph (copies structure and name).
         *
         * @param other  Source graph.
         */
        Graph(const Graph &other)
            : DiGraph(other), name_(other.name())
        {
        }

        /**
         * Get the current graph name.
         *
         * @return Name string.
         */
        std::string name() const
        {
            return name_;
        }

        /**
         * Set a new graph name.
         *
         * @param name  New name string.
         */
        void rename(const std::string name)
        {
            name_ = name;
        }

        /**
         * Add an imaginary (auxiliary) edge with explicit default properties.
         * The edge is marked as Imaginary, has weight 0 and buffer_id -1.
         *
         * @param parent  Source vertex id.
         * @param child   Target vertex id.
         */
        void addImEdge(size_t parent, size_t child)
        {
            auto res = boost::add_edge(parent, child, *this);
            if (res.second) {
                auto e = res.first;
                boost::put(edge_buffer_id_t(), *this, e, -1);
                boost::put(boost::edge_weight, *this, e, 0);
                boost::put(edge_kind_t(),      *this, e, EdgeKind::Imaginary);
            }
        }

        /**
         * Copy assignment: copies structure and name.
         *
         * @param other  Source graph.
         * @return Copied graph (by value).
         */
        Graph operator=(const Graph &other)
        {
            name_ = other.name();
            DiGraph::operator=(other);
            return *this;
        }
    };

    /** Vertex descriptor type for Graph. */
    typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;

    /** Property map for vertex indices (required by several Boost algorithms). */
    typedef boost::property_map<Graph, boost::vertex_index_t>::type IndexMap;

    /** Predecessor map type used in dominator-tree and similar algorithms. */
    typedef boost::iterator_property_map<std::vector<Vertex>::iterator, IndexMap> PredMap;

    /**
     * Binary tree node used by auxiliary algorithms (e.g., series-parallel detection).
     * Encapsulates a set of vertices and a state flag with optional left/right children.
     */
    class Node
    {
    public:
        /** Node type/state marker (implementation-specific). */
        char state;
        /** Set of vertex ids contained in this node. */
        std::unordered_set<int> vertices;
        /** Left child pointer (may be null). */
        Node *left;
        /** Right child pointer (may be null). */
        Node *right;

        /**
         * Construct a Node with a vertex set and optional children/state.
         *
         * @param vertices_  Vertex id set to store in this node.
         * @param left_      Left child pointer (default: nullptr).
         * @param right_     Right child pointer (default: nullptr).
         * @param state_     State marker (default: 'N').
         */
        Node(std::unordered_set<int> &vertices_, Node *left_ = nullptr, Node *right_ = nullptr, char state_ = 'N')
        {
            vertices = vertices_;
            left = left_;
            right = right_;
            state = state_;
        }
    };

    /**
     * A topological cut representation splitting vertices into S and T.
     * Holds cut partitions and accumulated weights for each side.
     */
    class topological_cut
    {
    public:
        /** Left partition of the cut. */
        std::set<int> S;
        /** Right partition of the cut. */
        std::set<int> T;
        /** Accumulated weight of S. */
        int s_weight = 0;
        /** Accumulated weight of T. */
        int t_weigt = 0;
    };
};
