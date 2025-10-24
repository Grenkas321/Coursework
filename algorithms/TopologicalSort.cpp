#include "TopologicalSort.h"

namespace scheduling_problem
{
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