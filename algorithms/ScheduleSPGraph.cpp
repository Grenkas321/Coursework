#include "ScheduleSPGraph.h"

#include <unordered_set>
#include <queue>
#include <climits>

namespace scheduling_problem::algorithms
{
    /**
     * Build a subgraph induced by the vertices contained in a series-parallel tree node.
     *
     * Copies edges (and their endpoint vertex weights/ids) from @p graph
     * only if both endpoints are present in @p root->vertices.
     *
     * @param graph  Original graph.
     * @param root   Pointer to the series-parallel decomposition node describing the vertex set.
     * @return       Induced subgraph on root->vertices (empty if root is null).
     */
    Graph make_subgraph(const Graph &graph, Node *root)
    {
        Graph sub_graph;

        if (!root)
        {
            return sub_graph;
        }

        for (auto edge : boost::make_iterator_range(boost::edges(graph)))
        {
            auto source_num = boost::get(vertex_num_t(), graph, edge.m_source);
            auto target_num = boost::get(vertex_num_t(), graph, edge.m_target);

            if (root->vertices.find(source_num) != root->vertices.end() && root->vertices.find(target_num) != root->vertices.end())
            {
                auto new_edge = boost::add_edge(source_num, target_num, sub_graph);

                boost::put(vertex_num_t(), sub_graph, new_edge.first.m_source, source_num);
                boost::put(vertex_num_t(), sub_graph, new_edge.first.m_target, target_num);

                auto source_weight = boost::get(vertex_weight_t(), graph, edge.m_source);
                auto target_weight = boost::get(vertex_weight_t(), graph, edge.m_target);

                boost::put(vertex_weight_t(), sub_graph, new_edge.first.m_source, source_weight);
                boost::put(vertex_weight_t(), sub_graph, new_edge.first.m_target, target_weight);
            }
        }

        return sub_graph;
    }

    /**
     * Linearize two parallel chains into a single chain preserving peak usage.
     *
     * Creates a new graph that concatenates the order of tasks in @p schedule1
     * and then inserts interior tasks of @p schedule2 between its first and last
     * vertices, finally connecting to the last vertex of @p schedule1. Vertex
     * weights are copied from @p graph using vertex_num_t mapping.
     *
     * @param graph     Source graph containing weights and vertex numbers.
     * @param schedule1 First chain (kept in order, defines first/last).
     * @param schedule2 Second chain (interior inserted between first/last).
     * @return          Linearized graph.
     */
    Graph linearize(const Graph &graph, ScheduleStatus &schedule1, ScheduleStatus &schedule2)
    {
        Graph linear_graph;
        std::map<int, int> node_nums;
        Graph::vertex_descriptor first = boost::graph_traits<Graph>::null_vertex();
        Graph::vertex_descriptor last  = boost::graph_traits<Graph>::null_vertex();
        Graph::vertex_descriptor cur   = boost::graph_traits<Graph>::null_vertex();
        Graph::vertex_descriptor prev  = boost::graph_traits<Graph>::null_vertex();

        // add schedule1 tasks as a chain
        for (auto task : schedule1)
        {
            cur = boost::add_vertex(linear_graph);
            last = cur;
            node_nums[task.id] = cur;
            boost::put(vertex_num_t(), linear_graph, cur, task.id);

            if (first == boost::graph_traits<Graph>::null_vertex())
            {
                first = cur;
            }
            else
            {
                boost::add_edge(prev, cur, linear_graph);
            }
            prev = cur;
        }

        // add interior tasks of schedule2 between 'first' and 'last'
        prev = first;

        for (size_t i = 1; i < schedule2.size() - 1; ++i)
        {
            Job task = schedule2[i];
            cur = boost::add_vertex(linear_graph);
            node_nums[task.id] = cur;
            boost::put(vertex_num_t(), linear_graph, cur, task.id);
            boost::add_edge(prev, cur, linear_graph);
            prev = cur;
        }

        boost::add_edge(prev, last, linear_graph);

        // assign vertex weights from the original graph
        for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
        {
            auto weight = boost::get(vertex_weight_t(), graph, vertex);
            auto num = boost::get(vertex_num_t(), graph, vertex);

            if (node_nums.find(num) != node_nums.end())
            {
                boost::put(vertex_weight_t(), linear_graph, node_nums[num], weight);
            }
        }

        return linear_graph;
    }

    /**
     * Post-order accumulation DFS for a tree-like graph rooted at @p cur_node.
     *
     * Sets vertex_weight(cur_node) = own weight + sum of weights of all immediate
     * predecessors (in-neighbors) after they have been processed. Leaves (with
     * zero in-degree) remain unchanged.
     *
     * @param g         Graph to update in place.
     * @param cur_node  Current node to process.
     */
    void dfs(Graph &g, int cur_node)
    {
        if (!boost::in_degree(cur_node, g))
        {
            return;
        }

        for (auto it : boost::make_iterator_range(boost::in_edges(cur_node, g)))
        {
            dfs(g, it.m_source);
        }

        int w{};
        for (auto it : boost::make_iterator_range(boost::in_edges(cur_node, g)))
        {
            w += boost::get(vertex_weight_t(), g, it.m_source);
        }

        w += boost::get(vertex_weight_t(), g, cur_node);

        boost::put(vertex_weight_t(), g, cur_node, w);
    }

    /**
     * Recompute cumulative subtree weights with the root adjusted to exclude its original weight.
     *
     * Produces a copy of @p g, performs @c dfs to accumulate weights, then
     * subtracts the original root weight from the recomputed root value.
     *
     * @param g     Input graph.
     * @param root  Root vertex descriptor.
     * @return      Graph with recomputed weights.
     */
    Graph recompute_weights(const Graph &g, int root)
    {
        Graph new_g = g;
        dfs(new_g, root);

        int root_w = boost::get(vertex_weight_t(), g, root);
        int cur_root_w = boost::get(vertex_weight_t(), new_g, root);
        boost::put(vertex_weight_t(), new_g, root, cur_root_w - root_w);

        return new_g;
    }

    /**
     * Compute an optimal schedule for a rooted tree.
     *
     * Returns a sequence of pairs {vertex_num, cumulative_weight} representing
     * an optimal order (for the tree case) and the running resource usage after
     * each vertex. The routine recursively schedules subtrees, merges them by
     * “hill/valley” segments to minimize the peak, and finally appends the root.
     *
     * @param g     Tree graph.
     * @param root  Root vertex.
     * @return      Vector of (vertex number, cumulative usage).
     */
    std::vector<std::pair<int, int>> Tree_schedule(const Graph &g, Graph::vertex_descriptor root)
    {
        if (boost::in_degree(root, g) == 0)
        {
            // last vertex
            auto weight = boost::get(vertex_weight_t(), g, root);
            auto num = boost::get(vertex_num_t(), g, root);
            std::vector<std::pair<int, int>> res = {std::make_pair((int)num, weight)};

            return res;
        }

        // pair<vertex number, vertex weight>
        std::vector<std::vector<std::pair<int, int>>> subschedules;

        for (auto e : boost::make_iterator_range(boost::in_edges(root, g)))
        {
            subschedules.push_back(Tree_schedule(g, e.m_source));
        }

        // compute hill/valley segments for each subschedule
        std::vector<std::vector<int>> hill_valley_segments;

        for (auto &sch : subschedules)
        {
            hill_valley_segments.push_back({});
            auto l_bound = sch.rend();

            while (std::distance(l_bound, sch.rbegin()) < 0)
            {
                auto hill_pos = std::max_element(sch.rbegin(), l_bound, [](std::pair<int, int> l, std::pair<int, int> r)
                                                 { return l.second < r.second; });
                l_bound = hill_pos;
                auto valley_pos = std::min_element(sch.rbegin(), l_bound + 1, [](std::pair<int, int> l, std::pair<int, int> r)
                                                   { return l.second < r.second; });
                l_bound = valley_pos;
                hill_valley_segments[hill_valley_segments.size() - 1].push_back(-std::distance(sch.rend(), hill_pos) - 1);
                hill_valley_segments[hill_valley_segments.size() - 1].push_back(-std::distance(sch.rend(), valley_pos) - 1);
            }
        }

        size_t scheduled_subtr = 0;
        std::vector<int> start(subschedules.size());
        std::vector<std::pair<int, int>> schedule;

        while (scheduled_subtr != subschedules.size())
        {
            int max_hv = 0;
            int max_hv_sch = 0;

            for (size_t i = 0; i < hill_valley_segments.size(); ++i)
            {
                if (static_cast<size_t>(start[i]) < hill_valley_segments[i].size())
                {

                    int cur_hv = subschedules[i][hill_valley_segments[i][start[i]]].second - subschedules[i][hill_valley_segments[i][start[i] + 1]].second;

                    if (cur_hv >= max_hv)
                    {
                        max_hv = cur_hv;
                        max_hv_sch = i;
                    }
                }
            }

            int l_shift = start[max_hv_sch] > 0 ? hill_valley_segments[max_hv_sch][start[max_hv_sch] - 1] + 1 : 0;
            int r_shift = hill_valley_segments[max_hv_sch][start[max_hv_sch] + 1];

            schedule.insert(schedule.end(), subschedules[max_hv_sch].begin() + l_shift, subschedules[max_hv_sch].begin() + r_shift + 1);
            start[max_hv_sch] += 2;

            if (static_cast<size_t>(start[max_hv_sch]) >= hill_valley_segments[max_hv_sch].size())
            {
                scheduled_subtr++;
            }
        }

        // recompute cumulative resource usage along the merged schedule
        std::unordered_map<int, int> match;

        for (auto it : boost::make_iterator_range(boost::vertices(g)))
        {
            match[boost::get(vertex_num_t(), g, it)] = it;
        }

        for (size_t pos = 0; pos < schedule.size(); ++pos)
        {
            int w = boost::get(vertex_weight_t(), g, match[schedule[pos].first]);

            if (pos > 0)
            {
                w += schedule[pos - 1].second;
            }

            for (auto in_e : boost::make_iterator_range(boost::in_edges(match[schedule[pos].first], g)))
            {
                w -= boost::get(vertex_weight_t(), g, in_e.m_source);
            }

            schedule[pos].second = w;
        }

        auto node = boost::get(vertex_num_t(), g, root);
        auto weight = boost::get(vertex_weight_t(), g, root);

        if (schedule.size() > 0)
        {
            weight += schedule[schedule.size() - 1].second;

            for (auto in_e : boost::make_iterator_range(boost::in_edges(root, g)))
            {
                weight -= boost::get(vertex_weight_t(), g, in_e.m_source);
            }
        }

        schedule.push_back(std::make_pair(node, weight));

        return schedule;
    }

    /**
     * Schedule a series-parallel graph given a minimum topological cut.
     *
      * Builds two trees: a reversed tree over S and a forward tree over T,
      * recomputes weights, obtains optimal tree schedules, and concatenates
      * them to form the final sequence expressed as {vertex_num, cumulative_weight}.
     *
     * @param g    Input graph.
     * @param cut  Minimal topological cut (S, T, weights).
     * @return     Concatenated schedule over the two parts.
     */
    std::vector<std::pair<int, int>> FJ_schedule(const Graph &g, topological_cut &cut)
    {
        // need reverse graph, which nodes stored in cut.S
        Graph tree_s, tree_t;

        // tree_s is reversed
        // tree_t is directed properly

        std::map<int, int> trees_num, treet_num;

        for (auto it : cut.S)
        {
            auto v = boost::add_vertex(tree_s);
            trees_num[it] = v;
            boost::put(vertex_num_t(), tree_s, v, it);
        }

        for (auto it : cut.T)
        {
            auto v = boost::add_vertex(tree_t);
            treet_num[it] = v;
            boost::put(vertex_num_t(), tree_t, v, it);
        }

        for (auto e : boost::make_iterator_range(boost::edges(g)))
        {
            auto source = boost::get(vertex_num_t(), g, e.m_source);
            auto target = boost::get(vertex_num_t(), g, e.m_target);
            auto source_weight = boost::get(vertex_weight_t(), g, e.m_source);
            auto target_weight = boost::get(vertex_weight_t(), g, e.m_target);

            if (cut.S.find(source) != cut.S.end() && cut.S.find(target) != cut.S.end())
            {
                boost::add_edge(trees_num[target], trees_num[source], tree_s);
                boost::put(vertex_weight_t(), tree_s, trees_num[source], -source_weight);
                boost::put(vertex_weight_t(), tree_s, trees_num[target], -target_weight);
            }
            else if (cut.T.find(source) != cut.T.end() && cut.T.find(target) != cut.T.end())
            {
                boost::add_edge(treet_num[source], treet_num[target], tree_t);
                boost::put(vertex_weight_t(), tree_t, treet_num[source], source_weight);
                boost::put(vertex_weight_t(), tree_t, treet_num[target], target_weight);
            }
        }

        Graph::vertex_descriptor root_s = boost::graph_traits<Graph>::null_vertex();
        Graph::vertex_descriptor root_t = boost::graph_traits<Graph>::null_vertex();

        // find roots of both trees
        for (auto it : boost::make_iterator_range(boost::vertices(tree_s)))
        {
            if (!boost::out_degree(it, tree_s))
            {
                root_s = it;
                break;
            }
        }

        for (auto it : boost::make_iterator_range(boost::vertices(tree_t)))
        {
            if (!boost::out_degree(it, tree_t))
            {
                root_t = it;
                break;
            }
        }

        tree_s = recompute_weights(tree_s, root_s);
        tree_t = recompute_weights(tree_t, root_t);

        auto sch1 = Tree_schedule(tree_s, root_s);
        auto sch2 = Tree_schedule(tree_t, root_t);

        std::reverse(sch1.begin(), sch1.end());
        sch1.insert(sch1.end(), sch2.begin(), sch2.end());
        return sch1;
    }

    /**
     * Schedule a series-parallel graph defined by its decomposition tree.
     *
     * Recursively schedules left/right subgraphs, then:
     *  - For 'S' (series) nodes, concatenates child schedules and merges cuts.
     *  - For 'P' (parallel) nodes, linearizes child schedules and applies FJ_schedule.
     * For a single-edge node ('N'), returns [u, v] and the corresponding cut.
     *
     * @param cur_graph  Current (sub)graph.
     * @param root       Root of the decomposition subtree.
     * @return           Pair of {ScheduleStatus, topological_cut}.
     */
    std::pair<ScheduleStatus, topological_cut> schedule_sp_graph(const Graph &cur_graph, Node *root)
    {
        ScheduleStatus sch(cur_graph);
        topological_cut cut;

        if (!root)
        {
            return std::make_pair(sch, cut);
        }

        if (root->state == 'N')
        {
            // one edge (a, b): schedule is [a, b]
            auto edge = *(boost::make_iterator_range(boost::edges(cur_graph)).begin());

            sch.insert(edge.m_source, sch.size(), cur_graph);
            sch.insert(edge.m_target, sch.size(), cur_graph);

            cut.S.insert(edge.m_source);
            cut.T.insert(edge.m_target);
            cut.s_weight = boost::get(vertex_weight_t(), cur_graph, edge.m_source);
            cut.t_weigt = boost::get(vertex_weight_t(), cur_graph, edge.m_target);

            return std::make_pair(sch, cut);
        }

        Graph g1 = make_subgraph(cur_graph, root->left);
        Graph g2 = make_subgraph(cur_graph, root->right);

        auto res1 = schedule_sp_graph(g1, root->left);
        auto res2 = schedule_sp_graph(g2, root->right);

        if (root->state == 'S')
        {
            // series composition of two subgraphs
            // duplicate of the boundary vertex counted twice; compensate
            int duplicate_weight = boost::get(vertex_weight_t(), cur_graph, res1.first[res1.first.size() - 1].id);

            if (res1.second.s_weight < res1.second.s_weight + res1.second.t_weigt + res2.second.s_weight - duplicate_weight)
            {
                cut.S = res1.second.S;
                cut.T = res1.second.T;
                cut.T.insert(res2.second.S.begin(), res2.second.S.end());
                cut.T.insert(res2.second.T.begin(), res2.second.T.end());
                cut.s_weight = res1.second.s_weight;
                cut.t_weigt = res1.second.t_weigt + res2.second.s_weight + res2.second.t_weigt - duplicate_weight;
            }
            else
            {
                cut.S = res1.second.S;
                cut.S.insert(res1.second.T.begin(), res1.second.T.end());
                cut.S.insert(res2.second.S.begin(), res2.second.S.end());
                cut.T = res2.second.T;
                cut.s_weight = res1.second.s_weight + res1.second.t_weigt + res2.second.s_weight - duplicate_weight;
                cut.t_weigt = res2.second.t_weigt;
            }

            sch = res1.first;

            for (int i = 1, end = res2.first.size(); i < end; ++i)
            {
                sch.insert(res2.first[i].id, sch.size(), cur_graph);
            }
        }

        else if (root->state == 'P')
        {
            // parallel composition of two subgraphs
            Graph linearized_graph = linearize(cur_graph, res1.first, res2.first);

            cut.S = res1.second.S;
            cut.S.insert(res2.second.S.begin(), res2.second.S.end());
            cut.T = res1.second.T;
            cut.T.insert(res2.second.T.begin(), res2.second.T.end());
            cut.s_weight = res1.second.s_weight + res2.second.s_weight;
            cut.t_weigt = res1.second.t_weigt + res2.second.t_weigt;

            auto tmp = FJ_schedule(linearized_graph, cut);

            for (auto it : tmp)
            {
                sch.insert(it.first, sch.size(), cur_graph);
            }
        }

        return std::make_pair(sch, cut);
    }

    /**
     * Initialize vertex_num_t property with each vertex's own descriptor value.
     *
     * @param graph  Graph whose vertex_num_t will be set to v for every vertex v.
     */
    void put_vertex_num(Graph &graph)
    {
        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            boost::put(vertex_num_t(), graph, v, v);
        }
    }

}
