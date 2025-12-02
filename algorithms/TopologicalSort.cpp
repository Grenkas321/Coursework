#include "TopologicalSort.h"

namespace scheduling_problem
{
    /**
 * @brief DFS helper for topological sorting (post-order push).
 *
 * Marks the current @p node as visited, recursively explores all outgoing
 * neighbors, then appends @p node to @p ans. Used by topo_sort to build a
 * finishing-time order that is later reversed.
 *
 * @param graph  Input graph.
 * @param ans    Output vector collecting nodes in DFS post-order.
 * @param used   Visitation flags; must be indexable by vertex id.
 * @param node   Start vertex id for this DFS call.
 */
    void dfs(const Graph &graph, std::vector<int> &ans, std::vector<bool> &used, int node)
    {
        used[node] = 1;

        for (auto it : boost::make_iterator_range(boost::out_edges(node, graph)))
        {
            if (!used[it.m_target])
            {
                dfs(graph, ans, used, it.m_target);
            }
        }

        ans.push_back(node);
    }

    /**
 * @brief Compute a topological order of the graph using DFS finishing times.
 *
 * Performs DFS from every unvisited vertex, collects vertices in post-order,
 * then reverses the sequence to obtain a topological ordering.
 * Complexity: O(V + E). Assumes vertices are addressable by integer ids.
 *
 * @param graph  Input DAG.
 * @return Vector of vertex ids in topological order.
 */
    std::vector<int> topo_sort(const Graph &graph)
    {
        std::vector<bool> used(boost::num_vertices(graph) + 1);
        std::vector<int> ans;

        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            if (!used[v])
            {
                dfs(graph, ans, used, v);
            }
        }

        std::reverse(ans.begin(), ans.end());

        return ans;
    }
}
