#include "GreedyEdgeVertexInsertion.h"

extern int first_peak_mem;

int reorder = 0, no_reorder = 0;

namespace scheduling_problem::algorithms
{
    /**
     * Depth-first search over outgoing edges.
     *
     * Marks all vertices reachable from v following out-edges.
     *
     * @param g        Graph to traverse.
     * @param visited  Set of visited vertices (updated in-place).
     * @param v        Start vertex.
     */
    void dfs(const Graph &g, std::unordered_set<int> &visited, int v)
    {
        visited.insert(v);

        for (auto it : boost::make_iterator_range(boost::out_edges(v, g)))
        {
            if (visited.find(it.m_target) == visited.end())
            {
                dfs(g, visited, it.m_target);
            }
        }
    }

    /**
     * Depth-first search over incoming edges (i.e., in the reversed graph).
     *
     * Marks all vertices that can reach v following in-edges.
     *
     * @param g        Graph to traverse (conceptually reversed).
     * @param visited  Set of visited vertices (updated in-place).
     * @param v        Start vertex.
     */
    void rev_dfs(const Graph &g, std::unordered_set<int> &visited, int v)
    {
        visited.insert(v);

        for (auto it : boost::make_iterator_range(boost::in_edges(v, g)))
        {
            if (visited.find(it.m_source) == visited.end())
            {
                rev_dfs(g, visited, it.m_source);
            }
        }
    }

    /**
     * Peak edge-based memory usage for a schedule.
     *
     * Model:
     * - On visiting task u, add weights of all out-edges (u, v).
     * - Track the maximum accumulated value.
     * - After the task, subtract weights of all in-edges (w, u).
     *
     * @param graph     Input graph (edge weights are memory).
     * @param schedule  Schedule to evaluate.
     * @return          Maximum memory consumption over the run.
     */
    int max_edge_mem_usage(const Graph &graph, const ScheduleStatus &schedule)
    {
        int mem_usage = 0, max_mem_usage = 0;
        for (auto task : schedule)
        {
            for (auto out_e : boost::make_iterator_range(boost::out_edges(task.id, graph)))
            {
                mem_usage += boost::get(boost::edge_weight_t(), graph, out_e);
            }

            if (mem_usage > max_mem_usage)
            {
                max_mem_usage = mem_usage;
            }

            for (auto in_e : boost::make_iterator_range(boost::in_edges(task.id, graph)))
            {
                mem_usage -= boost::get(boost::edge_weight_t(), graph, in_e);
            }
        }

        return max_mem_usage;
    }

    /**
     * Peak vertex-based memory usage for a schedule.
     *
     * Model:
     * - On visiting task u, add vertex weight w(u).
     * - Once all children of a vertex p have been scheduled, subtract w(p).
     *
     * @param graph     Input graph (vertex weights are memory).
     * @param schedule  Schedule to evaluate.
     * @return          Maximum memory consumption over the run.
     */
    int max_vertex_mem_usage(const Graph &graph, const ScheduleStatus &schedule)
    {
        int mem_usage = 0, max_mem_usage = 0;
        std::unordered_map<int, std::unordered_set<int>> tasks_to_complete;

        for (auto v : boost::make_iterator_range(boost::vertices(graph)))
        {
            for (auto e : boost::make_iterator_range(boost::out_edges(v, graph)))
            {
                tasks_to_complete[v].insert(e.m_target);
            }
        }

        for (auto task : schedule)
        {
            mem_usage += boost::get(vertex_weight_t(), graph, task.id);

            if (mem_usage > max_mem_usage)
            {
                max_mem_usage = mem_usage;
            }

            for (auto it : tasks_to_complete)
            {
                if (it.second.find(task.id) != it.second.end())
                {
                    tasks_to_complete[it.first].erase(task.id);

                    if (tasks_to_complete[it.first].size() == 0)
                    {
                        mem_usage -= boost::get(vertex_weight_t(), graph, it.first);
                        tasks_to_complete.erase(it.first);
                    }
                }
            }
        }

        return max_mem_usage;
    }

    /**
     * Find bounds around a vertex in the current schedule.
     *
     * Computes:
     * - iterator to the latest scheduled predecessor of `vertex`,
     * - iterator to the earliest scheduled successor of `vertex`.
     *
     * @param graph     Input graph.
     * @param schedule  Current schedule.
     * @param vertex    Vertex of interest.
     * @return          Pair {last_pred_pos, first_succ_pos}.
     */
    std::pair<ScheduleStatus::const_iterator, ScheduleStatus::const_iterator> find_pred_anc(const Graph &graph,
                                                                                            const ScheduleStatus &schedule,
                                                                                            Graph::vertex_descriptor vertex)
    {
        auto vertex_anc_pos = schedule.end(), vertex_pred_pos = schedule.begin();

        for (auto out_edge : boost::make_iterator_range(boost::out_edges(vertex, graph)))
        {
            auto cur_anc_pos = schedule.loc(out_edge.m_target) + schedule.begin();

            if (cur_anc_pos < vertex_anc_pos)
            {
                vertex_anc_pos = cur_anc_pos;
            }
        }

        for (auto in_edge : boost::make_iterator_range(boost::in_edges(vertex, graph)))
        {
            auto cur_pred_pos = schedule.loc(in_edge.m_source) + schedule.begin();

            if (cur_pred_pos > vertex_pred_pos)
            {
                vertex_pred_pos = cur_pred_pos;
            }
        }

        return std::make_pair(vertex_pred_pos, vertex_anc_pos);
    }

    /**
     * Insert an edge (source -> target) into a subgraph, adjusting the schedule if needed.
     *
     * If source already precedes target in the schedule:
     *   - If target is the last successor of source, explore moving `target`
     *     within [after last predecessor of target .. current position] to
     *     reduce peak memory; otherwise return schedule unchanged.
     * If source follows target (ordering violation):
     *   - Attempt to reorder limited blocks of predecessors/successors to fix
     *     the violation and minimize peak memory, ensuring correctness.
     *
     * @param graph    Full graph (for correctness/memory checks).
     * @param schedule Current schedule on which to operate.
     * @param source   Edge source.
     * @param target   Edge target.
     * @return         Updated schedule.
     */
    ScheduleStatus add_edge(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor source, Graph::vertex_descriptor target)
    {
        auto source_pos = schedule.begin() + schedule.loc(source);
        auto target_pos = schedule.begin() + schedule.loc(target);

        if (source_pos < target_pos)
        {
            int cur_peak_mem = peak_memory(graph, schedule);

            std::vector<int> succ_pos;

            for (auto out_e : boost::make_iterator_range(boost::out_edges(source, graph)))
            {
                succ_pos.push_back(schedule.loc(out_e.m_target));
            }

            std::sort(succ_pos.begin(), succ_pos.end());

            /**
             * If target is currently the last scheduled successor of source,
             * try moving it left towards its last predecessor to lower the peak.
             */
            if (succ_pos.size() && (target_pos - schedule.begin() == succ_pos[succ_pos.size() - 1]))
            {
                auto last_target_pred = schedule.begin();

                for (auto in_e : boost::make_iterator_range(boost::in_edges(target, graph)))
                {
                    last_target_pred = std::max(last_target_pred, schedule.loc(in_e.m_source) + schedule.begin());
                }

                ScheduleStatus new_schedule(graph);
                ScheduleStatus new_best_schedule = schedule;

                for (auto it = last_target_pred + 1; it <= target_pos; ++it)
                {
                    new_schedule.clear();

                    for (auto i = schedule.begin(); i < it; ++i)
                    {
                        new_schedule.insert(i->id, new_schedule.size(), graph);
                    }
                    new_schedule.insert(target, new_schedule.size(), graph);
                    for (auto i = it; i < schedule.end(); ++i)
                    {
                        if (i != target_pos)
                        {
                            new_schedule.insert(i->id, new_schedule.size(), graph);
                        }
                    }

                    auto new_peak_mem = peak_memory(graph, new_schedule);

                    if (new_peak_mem < cur_peak_mem)
                    {
                        cur_peak_mem = new_peak_mem;
                        new_best_schedule = new_schedule;
                    }
                }

                return new_best_schedule;
            }

            return schedule;
        }

        reorder++;

        /**
         * Collect sets needed for constrained reordering:
         * - target_pred: predecessors of target (via reverse DFS),
         * - target_succ: successors of target,
         * - source_succ: successors of source.
         */
        std::unordered_set<int> target_pred;
        rev_dfs(graph, target_pred, target);

        std::unordered_set<int> target_succ;
        dfs(graph, target_succ, target);

        std::unordered_set<int> source_succ;
        dfs(graph, source_succ, source);

        std::vector<int> target_pred_shift;
        std::vector<int> source_succ_shift;
        ScheduleStatus best_schedule(graph);
        int min_peak_mem = INT_MAX;

        /**
         * Try shifting a prefix of target's predecessors left of target,
         * and a prefix of source's successors right of source, rebuilding
         * a candidate schedule and keeping the best feasible one.
         */
        for (int t = target_pos - schedule.begin(); t >= 0; --t)
        {
            source_succ_shift.clear();

            if (target_pred.find(schedule[t].id) != target_pred.end())
            {
                target_pred_shift.push_back(schedule[t].id);

                bool bad_edge_fl{0};
                Graph::edge_descriptor bad_edge;

                for (size_t s = static_cast<size_t>(source_pos - schedule.begin()); s < schedule.size(); ++s)
                {
                    if (source_succ.find(schedule[s].id) != source_succ.end())
                    {
                        source_succ_shift.push_back(schedule[s].id);

                        ScheduleStatus new_schedule(graph);

                        for (int i = 0; i < target_pos - schedule.begin(); ++i)
                        {
                            if (i < t ||
                                target_pred.find(schedule[i].id) == target_pred.end())
                            {
                                new_schedule.insert(schedule[i].id, new_schedule.size(), graph);
                            }
                        }

                        std::vector<int> tasks_to_shift;

                        for (int i = target_pos - schedule.begin() + 1; i < source_pos - schedule.begin(); ++i)
                        {
                            if (target_succ.find(schedule[i].id) != target_succ.end())
                            {
                                tasks_to_shift.push_back(schedule[i].id);
                            }
                            else
                            {
                                new_schedule.insert(schedule[i].id, new_schedule.size(), graph);
                            }
                        }

                        for (auto job : source_succ_shift)
                        {
                            new_schedule.insert(job, new_schedule.size(), graph);
                        }

                        for (auto job : target_pred_shift)
                        {
                            new_schedule.insert(job, new_schedule.size(), graph);
                        }

                        for (auto job : tasks_to_shift)
                        {
                            new_schedule.insert(job, new_schedule.size(), graph);
                        }

                        for (size_t i = static_cast<size_t>(source_pos - schedule.begin()); i < schedule.size(); ++i)
                        {
                            if (i > s ||
                                source_succ.find(schedule[i].id) == source_succ.end())
                            {
                                new_schedule.insert(schedule[i].id, new_schedule.size(), graph);
                            }
                        }

                        /**
                         * If we previously detected a bad edge, verify the candidate fixes it.
                         */
                        if (bad_edge_fl)
                        {
                            auto pos1 = new_schedule.loc(bad_edge.m_source);
                            auto pos2 = new_schedule.loc(bad_edge.m_target);

                            if (pos1 >= pos2)
                            {
                                continue;
                            }
                        }

                        /**
                         * Accept the candidate only if it is topologically correct,
                         * then minimize peak memory.
                         */
                        if (test_correctness(graph, new_schedule, bad_edge_fl, bad_edge))
                        {
                            bad_edge_fl = 0;

                            int cur_peak = peak_memory(graph, new_schedule);

                            if (cur_peak < min_peak_mem)
                            {
                                best_schedule = new_schedule;
                                min_peak_mem = cur_peak;
                            }
                        }
                    }
                }
            }
        }

        return best_schedule;
    }

    /**
     * Select the next edge to insert into the subgraph.
     *
     * For each candidate, temporarily add it, adapt the schedule using
     * `add_edge()`, and evaluate peak memory. Choose the edge that yields
     * the lowest peak.
     *
     * @param sub_g  Current subgraph (will be temporarily modified).
     * @param edges  Candidate edges to add.
     * @param sch    Current schedule.
     * @return       Descriptor of the chosen edge.
     */
    Graph::edge_descriptor next_edge(Graph &sub_g, const std::vector<Graph::edge_descriptor> &edges, const ScheduleStatus &sch)
    {
        std::map<Graph::edge_descriptor, int> cnt;

        for (auto it : edges)
        {
            boost::add_edge(it.m_source, it.m_target, sub_g);
            auto new_sch = add_edge(sub_g, sch, it.m_source, it.m_target);
            boost::remove_edge(it.m_source, it.m_target, sub_g);

            cnt[it] = peak_memory(sub_g, new_sch);
        }

        auto mn = std::min_element(cnt.begin(), cnt.end());

        return mn->first;
    }

    /**
     * Remove already satisfied edges from the candidate list and inject them into subgraph.
     *
     * An edge (u,v) is satisfied if u and v are present in the schedule and
     * loc(u) < loc(v). Such edges are added to `sub_g` (with proper properties)
     * and are excluded from the returned list.
     *
     * @param graph   Full graph (for weights).
     * @param sub_g   Subgraph being built (modified in-place).
     * @param edges   Initial list of candidate edges.
     * @param sch     Current schedule.
     * @return        Remaining unsatisfied edges.
     */
    std::vector<Graph::edge_descriptor> delete_satisfied(const Graph &graph, Graph &sub_g, const std::vector<Graph::edge_descriptor> &edges, const ScheduleStatus &sch)
    {
        std::vector<Graph::edge_descriptor> new_edges;

        for (auto it : edges)
        {
            if (sch.contains(it.m_source) && sch.contains(it.m_target) && sch.loc(it.m_source) < sch.loc(it.m_target))
            {
                if (!boost::edge(it.m_source, it.m_target, sub_g).second)
                {
                    auto w = boost::get(boost::edge_weight_t(), graph, it);

                    auto res = boost::add_edge(it.m_source, it.m_target, sub_g);
                    if (res.second) {
                        auto e = res.first;
                        boost::put(edge_buffer_id_t(), sub_g, e, 0);
                        boost::put(boost::edge_weight,  sub_g, e, w);
                        boost::put(edge_kind_t(),       sub_g, e, EdgeKind::Real);
                    }
                }
            }
            else
            {
                new_edges.push_back(it);
            }
        }

        return new_edges;
    }

    static std::vector<int> prev;
    static std::vector<int> cur;

    /**
     * Insert a vertex into the subgraph and adapt the schedule accordingly.
     *
     * If `parent == -1`, the vertex is inserted at the front and all existing
     * jobs are appended. Otherwise, add edge (parent, vertex) to the subgraph,
     * place `vertex` after `parent`, then progressively add other incident
     * edges choosing the best order by `next_edge()`.
     *
     * @param graph     Full graph.
     * @param sp_graph  Subgraph being built (modified in-place).
     * @param schedule  Current schedule to adapt.
     * @param vertex    Vertex to insert.
     * @param parent    Parent within subgraph, or -1 if none/entry.
     * @return          Updated schedule.
     */
    ScheduleStatus add_vertex(const Graph &graph, Graph &sp_graph, const ScheduleStatus &schedule, int vertex, int parent)
    {
        ScheduleStatus best_schedule(graph);

        if (parent == -1)
        {
            best_schedule.insert(vertex, 0, graph);
            for (auto job : schedule)
            {
                best_schedule.insert(job.id, best_schedule.size(), graph);
            }
        }
        else
        {
            auto pred_pos = schedule.loc(parent);

            auto w = boost::get(boost::edge_weight_t(), graph, boost::edge(parent, vertex, graph).first);
            auto res = boost::add_edge(parent, vertex, sp_graph);
            if (res.second) {
                auto e = res.first;
                boost::put(edge_buffer_id_t(), sp_graph, e, 0);
                boost::put(boost::edge_weight,  sp_graph, e, w);
                boost::put(edge_kind_t(),       sp_graph, e, EdgeKind::Real);
            }
            boost::put(vertex_weight_t(), sp_graph, vertex, w);

            for (auto job = schedule.begin(); job <= schedule.begin() + pred_pos; ++job)
            {
                best_schedule.insert(job->id, best_schedule.size(), graph);
            }

            best_schedule.insert(vertex, best_schedule.size(), graph);

            for (auto job = schedule.begin() + pred_pos + 1; job != schedule.end(); ++job)
            {
                best_schedule.insert(job->id, best_schedule.size(), graph);
            }
        }

        std::vector<Graph::edge_descriptor> edges;

        for (auto in_e : boost::make_iterator_range(boost::in_edges(vertex, graph)))
        {
            if (static_cast<long unsigned>(in_e.m_source) == static_cast<long unsigned>(parent) || !schedule.contains(in_e.m_source))
            {
                continue;
            }

            edges.push_back(in_e);
        }

        for (auto out_e : boost::make_iterator_range(boost::out_edges(vertex, graph)))
        {
            if (!schedule.contains(out_e.m_target))
            {
                continue;
            }

            edges.push_back(out_e);
        }

        edges = delete_satisfied(graph, sp_graph, edges, schedule);

        while (edges.size())
        {
            auto next = next_edge(sp_graph, edges, best_schedule);

            auto w = boost::get(boost::edge_weight_t(), graph, next);

            auto res = boost::add_edge(next.m_source, next.m_target, sp_graph);
            if (res.second) {
                auto e = res.first;
                boost::put(edge_buffer_id_t(), sp_graph, e, 0);
                boost::put(boost::edge_weight,  sp_graph, e, w);
                boost::put(edge_kind_t(),       sp_graph, e, EdgeKind::Real);
            }

            best_schedule = add_edge(sp_graph, best_schedule, next.m_source, next.m_target);

            edges = delete_satisfied(graph, sp_graph, edges, best_schedule);
        }

        return best_schedule;
    }

    /**
     * Check whether the schedule already contains at least one parent of `vertex`.
     *
     * @param graph     Full graph.
     * @param schedule  Current schedule.
     * @param vertex    Vertex of interest.
     * @return          Parent id if found, otherwise -1.
     */
    int parent_in_sp_graph(const Graph &graph, const ScheduleStatus &schedule, Graph::vertex_descriptor vertex)
    {
        for (auto it : boost::make_iterator_range(boost::in_edges(vertex, graph)))
        {
            if (schedule.contains(it.m_source))
            {
                return it.m_source;
            }
        }

        return -1;
    }

    static int cost = 0;
    static int cnt = 0;

    /**
     * Insert all missing edges/vertices into a subgraph and adapt the schedule.
     *
     * Steps:
     * 1) Add edges between already-scheduled pairs in an order minimizing peak memory.
     * 2) While there are unscheduled vertices:
     *      - pick a vertex whose parent exists in the schedule or is a source,
     *      - add it via `add_vertex`,
     *      - keep integrating incident edges using `next_edge`.
     *
     * @param graph     Full graph.
     * @param sp_graph  Subgraph being completed (passed by value to isolate edits).
     * @param schedule  Current schedule as a starting point.
     * @return          Completed schedule consistent with the full graph.
     */
    ScheduleStatus insert_vertices_and_edges(const Graph &graph, Graph sp_graph, const ScheduleStatus &schedule)
    {
        ScheduleStatus new_schedule = schedule;
        std::vector<Graph::edge_descriptor> edges;

        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (new_schedule.contains(e.m_source) && new_schedule.contains(e.m_target))
            {

                bool edge_in_sp = false;

                for (auto sp_edge : boost::make_iterator_range(boost::edges(sp_graph)))
                {
                    if (e.m_source == sp_edge.m_source && e.m_target == sp_edge.m_target)
                    {
                        edge_in_sp = true;
                        break;
                    }
                }

                if (!edge_in_sp)
                {
                    edges.push_back(e);
                }
            }
        }

        edges = delete_satisfied(graph, sp_graph, edges, new_schedule);

        while (edges.size())
        {
            auto next = next_edge(sp_graph, edges, new_schedule);

            auto w = boost::get(boost::edge_weight_t(), graph, next);

            auto res = boost::add_edge(next.m_source, next.m_target, sp_graph);
            if (res.second) {
                auto e = res.first;
                boost::put(edge_buffer_id_t(), sp_graph, e, 0);
                boost::put(boost::edge_weight,  sp_graph, e, w);
                boost::put(edge_kind_t(),       sp_graph, e, EdgeKind::Real);
            }

            new_schedule = add_edge(sp_graph, new_schedule, next.m_source, next.m_target);

            edges = delete_satisfied(graph, sp_graph, edges, new_schedule);
        }

        /**
         * Insert remaining vertices one by one, always respecting precedence.
         */
        while (new_schedule.size() != boost::num_vertices(graph))
        {
            for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
            {
                if (!new_schedule.contains(vertex))
                {
                    auto parent = parent_in_sp_graph(graph, new_schedule, vertex);

                    if (parent != -1 || boost::in_degree(vertex, graph) == 0)
                    {
                        if (boost::in_degree(vertex, graph) == 0)
                        {
                            parent = -1;
                        }

                        new_schedule = add_vertex(graph, sp_graph, new_schedule, vertex, parent);
                    }
                }
            }
        }

        prev = cur;
        cur = {};

        if (cnt == 5)
        {
            cnt = cost = 0;
            prev = cur = {};
        }

        return new_schedule;
    }
}
