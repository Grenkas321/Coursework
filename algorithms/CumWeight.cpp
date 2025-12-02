#include "CumWeight.h"

namespace scheduling_problem::algorithms
{
    /**
     * Transform an edge-weighted graph into a vertex-weighted graph for memory profiling.
     *
     * Construction:
     * - For each original vertex i, create two vertices: 2*i (start) and 2*i+1 (end).
     * - Add an internal edge (2*i -> 2*i+1) if the original vertex has any incident edges.
     * - For each original edge (u -> v), add an edge (2*u+1 -> 2*v) to connect
     *   the end of u with the start of v.
     * - Set vertex weights:
     *     weight(2*i)   = sum of weights of all outgoing edges of i,
     *     weight(2*i+1) = - sum of weights of all incoming edges of i.
     *
     * The resulting graph encodes cumulative memory changes as vertex weights.
     *
     * @param graph  Original edge-weighted graph.
     * @return       Vertex-weighted expanded graph.
     */
    Graph edge_to_vertex_mem(const Graph &graph)
    {
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

    /**
     * Rebuild a series/parallel decomposition node for the expanded graph.
     *
     * For a node containing original vertices {i}, replace its vertex set with
     * the expanded set {2*i, 2*i+1} for each i. If the node is a leaf-like
     * edge node (no children), create left/right children corresponding to
     * the two endpoints' expanded pairs and mark state 'S'.
     *
     * @param root  Root of the original decomposition subtree.
     * @return      Root updated to reference expanded-graph vertex ids.
     */
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

    /**
     * Convert a schedule over the expanded graph back to the original vertex ids.
     *
     * The expanded graph duplicates each original vertex i into (2*i, 2*i+1).
     * A clean schedule should contain only the "start" vertices (even ids).
     * This function scans the expanded schedule and inserts i at the end
     * whenever it encounters 2*i.
     *
     * @param graph     Original graph (for insert semantics).
     * @param schedule  Schedule built on the expanded graph.
     * @return          Schedule over original vertex ids.
     */
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
