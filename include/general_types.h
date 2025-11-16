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
 * Implemented algorithms for task graph scheduling
 */
namespace scheduling_problem
{
    /**
     * Defines node properties
     */
    struct vertex_weight_t
    {
        /**
         * Stores any additional inforamtion about the edge
         */
        typedef boost::vertex_property_tag kind;
    };

    struct vertex_num_t
    {
        typedef boost::vertex_property_tag kind;
    };

    /**
     * @brief Used to initiate node weights
     */
    typedef long long weight_t;

    typedef std::size_t num_t;

    /**
     * Properties of the graph node
     */
    typedef boost::property<vertex_weight_t, weight_t,
                            boost::property<vertex_num_t, num_t,
                                            boost::property<boost::vertex_index_t, std::size_t>>>
        VertexProperty;

    /**
     * Specifies edge structure
     */
    struct edge_kind_t
    {
        /**
         * Stores any additional inforamtion about the edge
         */
        typedef boost::edge_property_tag kind;
    };

    struct edge_buffer_id_t
    {
        typedef boost::edge_property_tag kind;
    };

    /**
     * Possible kinds of edges
     */
    enum class EdgeKind
    {
        /**
         * The edge originally exists in graph
         */
        Real = false,
        /**
         * The edge is imaginary, i.e. was added to the graph to assist the scheduling algorithms
         */
        Imaginary = true
    };

    /**
     * Edge properties defined
     */
    typedef boost::property<edge_kind_t,
                            EdgeKind,
                            boost::property<boost::edge_index_t, size_t>>
        EdgeKindProperty;

    /**
     * Edge weight defined
     */
    typedef boost::property<edge_buffer_id_t,
                        int,
                        boost::property<boost::edge_weight_t,
                                        weight_t,
                                        EdgeKindProperty>>
    EdgeProperties;

    /**
     * %Graph structure defined
     */
    typedef boost::adjacency_list<boost::vecS,
                                  boost::vecS,
                                  boost::bidirectionalS,
                                  VertexProperty,
                                  EdgeProperties>
        DiGraph;

    /**
     * @brief Inherits from boost::adjacency_list and includes additional fuctionality like adding imaginary edges
     */
    class Graph : public DiGraph
    {
    private:
        std::string name_;

    public:
        /**
         * Consturctor
         * @param n_vertex The number of nodes in graph
         * @param name Graph name
         */
        Graph(size_t n_vertex = 0,
              const std::string &name = "dag")
            : DiGraph(n_vertex), name_(name)
        {
        }
        /**
         * Copy constructor
         */
        Graph(const Graph &other)
            : DiGraph(other), name_(other.name())
        {
        }

        /**
         * Get name of graph
         * @return Graph name
         */
        std::string name() const
        {
            return name_;
        }

        /**
         * Rename graph
         * @param name New name
         */
        void rename(const std::string name)
        {
            name_ = name;
        }

        /**
         * @brief Adds an imaginary edge. Imaginary edges are used to make subareas
         *
         * @param parent Parent id (edge will start in this node)
         * @param child Child id (edge will end in this node)
         */
        void addImEdge(size_t parent, size_t child)
        {
            // добавляем ребро без свойств
            auto res = boost::add_edge(parent, child, *this);
            if (res.second) {
                auto e = res.first;
                // устанавливаем свойства ЯВНО:
                boost::put(edge_buffer_id_t(), *this, e, 0);           // фиктивному ребру — buffer_id=0
                boost::put(boost::edge_weight, *this, e, 0);            // вес 0 для Imaginary
                boost::put(edge_kind_t(),      *this, e, EdgeKind::Imaginary);
            }
        }
        /**
         * Assignment operator for graphs
         */
        Graph operator=(const Graph &other)
        {
            name_ = other.name();
            DiGraph::operator=(other);
            return *this;
        }
    };

    typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;

    typedef boost::property_map<Graph, boost::vertex_index_t>::type IndexMap;

    typedef boost::iterator_property_map<std::vector<Vertex>::iterator, IndexMap> PredMap;

    class Node
    {
    public:
        char state;
        std::unordered_set<int> vertices;
        Node *left;
        Node *right;

        Node(std::unordered_set<int> &vertices_, Node *left_ = nullptr, Node *right_ = nullptr, char state_ = 'N')
        {
            vertices = vertices_;
            left = left_;
            right = right_;
            state = state_;
        }
    };

    class topological_cut
    {
    public:
        std::set<int> S;
        std::set<int> T;
        int s_weight = 0;
        int t_weigt = 0;
    };
};
