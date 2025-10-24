#include "ScheduleSPGraph.h"

#include <unordered_set>
#include <queue>
#include <climits>

namespace scheduling_problem::algorithms
{
    /**
     * Create subgraph of the given graph with only such nodes that are present in the tree that is passed as second argument of the function
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
     * Transforms the given parallel graph that consists of two parallel chains into single chain such that the peak resource usage for both graphs will be the same
     */
    Graph linearize(const Graph &graph, ScheduleStatus &schedule1, ScheduleStatus &schedule2)
    {
        Graph linear_graph;
        std::map<int, int> node_nums;
        Graph::vertex_descriptor first = -1, last, cur, prev;

        // adding schedule1 tasks to graph

        for (auto task : schedule1)
        {
            cur = boost::add_vertex(linear_graph);
            last = cur;
            node_nums[task.id] = cur;
            boost::put(vertex_num_t(), linear_graph, cur, task.id);

            if (first == -1)
            {
                first = cur;
            }
            else
            {
                boost::add_edge(prev, cur, linear_graph);
            }
            prev = cur;
        }

        // adding schedul2 tasks to graph
        prev = first;

        for (int i = 1; i < schedule2.size() - 1; ++i)
        {
            Job task = schedule2[i];
            cur = boost::add_vertex(linear_graph);
            node_nums[task.id] = cur;
            boost::put(vertex_num_t(), linear_graph, cur, task.id);
            boost::add_edge(prev, cur, linear_graph);
            prev = cur;
        }

        boost::add_edge(prev, last, linear_graph);

        // set vertex weights

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
     * Depth first search for the given graph and start node v
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
     * Schedules a tree such that the resulting schedule will be optimal
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

        // for each subschedule we need to compute hill and valley point
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

        int scheduled_subtr{};
        std::vector<int> start(subschedules.size());
        std::vector<std::pair<int, int>> schedule;

        while (scheduled_subtr != subschedules.size())
        {
            int max_hv{};
            int max_hv_sch;

            for (int i = 0; i < hill_valley_segments.size(); ++i)
            {
                if (start[i] < hill_valley_segments[i].size())
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

            if (start[max_hv_sch] >= hill_valley_segments[max_hv_sch].size())
            {
                scheduled_subtr++;
            }
        }

        // need to recompute resource usage along schedule

        std::unordered_map<int, int> match;

        for (auto it : boost::make_iterator_range(boost::vertices(g)))
        {
            match[boost::get(vertex_num_t(), g, it)] = it;
        }

        for (int pos = 0; pos < schedule.size(); ++pos)
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
     * %Schedule %series-parallel %graph with the given minimal topological cut
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

        Graph::vertex_descriptor root_s, root_t;

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
            // we have one edge (a, b) ===> its schedule is already specified [a, b]
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
            // graph is a series composition of two subgraphs
            // here we count twice weight of last task in res1 and first task in res2, because its the same task
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
            // graph is a parallel composition of two subgrphs
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

    void put_vertex_num(Graph &graph)
    {
        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            boost::put(vertex_num_t(), graph, v, v);
        }
    }

}