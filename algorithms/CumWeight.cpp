#include "CumWeight.h"

namespace scheduling_problem::algorithms
{
    Graph edge_to_vertex_mem(const Graph &graph)
    {
        // for each vertex with number i from initial Graph we map two vertices 2 * i and 2 * i + 1
        // 2 * i vertex is a start vertex and 2 * i + 1 vertex is and end vertex

        Graph new_graph;

        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (boost::in_degree(v, graph) || boost::out_degree(v, graph))
            {
                boost::add_edge(2 * v, 2 * v + 1, new_graph);
            }
        }

        for (auto edge : boost::make_iterator_range(boost::edges(graph)))
        {
            boost::add_edge(2 * edge.m_source + 1, 2 * edge.m_target, new_graph);
        }

        for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
        {
            int in_weight = 0;
            int out_weight = 0;

            for (auto in_edge : boost::make_iterator_range(boost::in_edges(vertex, graph)))
            {
                in_weight += boost::get(boost::edge_weight_t(), graph, in_edge);
            }
            for (auto out_edge : boost::make_iterator_range(boost::out_edges(vertex, graph)))
            {
                out_weight += boost::get(boost::edge_weight_t(), graph, out_edge);
            }

            boost::put(vertex_weight_t(), new_graph, 2 * vertex, out_weight);
            boost::put(vertex_weight_t(), new_graph, 2 * vertex + 1, -in_weight);
        }

        return new_graph;
    }

    Node *RecomputeTree(Node *root)
    {
        if (!root)
        {
            return nullptr;
        }

        std::unordered_set<int> vertices;

        for (auto it : root->vertices)
        {
            vertices.insert(2 * it);
            vertices.insert(2 * it + 1);
        }

        if (!root->left || !root->right)
        {
            // its an edge and we need to add its children
            int u = -1, v = -1;
            for (auto it : root->vertices)
            {
                if (u < 0)
                {
                    u = it;
                }
                else
                {
                    v = it;
                }
            }

            std::unordered_set<int> l_vert({2 * u, 2 * u + 1});
            std::unordered_set<int> r_vert({2 * v, 2 * v + 1});

            Node *l = new Node(l_vert);
            Node *r = new Node(r_vert);

            root->left = l;
            root->right = r;
            root->state = 'S';
        }

        root->vertices = vertices;

        return root;
    }

    ScheduleStatus clean_cum_weight_schedule(const Graph &graph, const ScheduleStatus &schedule)
    {
        ScheduleStatus clean_schedule(graph);

        for (auto job : schedule)
        {
            if (job.id % 2 == 0)
            {
                clean_schedule.insert(job.id / 2, clean_schedule.size(), graph);
            }
        }

        return clean_schedule;
    }
}