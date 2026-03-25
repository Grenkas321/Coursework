#include "LayeredSchedule.h"

#include <algorithm>
#include <numeric>
#include <queue>

#include "BufferIndex.h"
#include "TopologicalSort.h"

namespace scheduling_problem::algorithms
{
    namespace
    {
        weight_t durationOf(const Graph &graph, size_t v)
        {
            auto exec = boost::get(vertex_exec_time_t(), graph);
            auto w = boost::get(vertex_weight_t(), graph);
            const auto d = exec[v];
            return d > 0 ? d : std::max<weight_t>(1, w[v]);
        }

        void rebuildIndices(LayeredState &state)
        {
            for (unsigned p = 0; p < state.processors.size(); ++p)
            {
                for (size_t t = 0; t < state.processors[p].size(); ++t)
                {
                    const auto task = state.processors[p][t];
                    if (task < state.proc_of.size())
                    {
                        state.proc_of[task] = p;
                        state.tier_of[task] = t;
                    }
                }
            }
        }
    }

    LayeredState makeListState(const Graph &graph, unsigned processors)
    {
        const size_t n = boost::num_vertices(graph);
        const unsigned pcount = std::max(1u, processors);

        LayeredState state;
        state.processors.assign(pcount, {});
        state.proc_of.assign(n, 0);
        state.tier_of.assign(n, 0);

        auto topo = topo_sort(graph);
        if (topo.size() != n)
        {
            topo.resize(n);
            std::iota(topo.begin(), topo.end(), 0);
        }

        std::vector<weight_t> proc_free(pcount, 0);
        std::vector<weight_t> finish(n, 0);

        for (const auto raw_v : topo)
        {
            const auto v = static_cast<size_t>(raw_v);
            weight_t dep_ready = 0;
            for (auto e : boost::make_iterator_range(boost::in_edges(v, graph)))
            {
                if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real)
                    continue;
                dep_ready = std::max(dep_ready, finish[boost::source(e, graph)]);
            }

            unsigned best_p = 0;
            weight_t best_start = std::numeric_limits<weight_t>::max();
            weight_t best_finish = std::numeric_limits<weight_t>::max();
            const auto dur = durationOf(graph, v);

            for (unsigned p = 0; p < pcount; ++p)
            {
                const auto start = std::max(dep_ready, proc_free[p]);
                const auto fin = start + dur;
                if (fin < best_finish || (fin == best_finish && start < best_start))
                {
                    best_p = p;
                    best_start = start;
                    best_finish = fin;
                }
            }

            state.proc_of[v] = best_p;
            state.tier_of[v] = state.processors[best_p].size();
            state.processors[best_p].push_back(v);
            proc_free[best_p] = best_finish;
            finish[v] = best_finish;
        }

        return state;
    }

    LayeredState makeStateFromSchedule(const Graph &graph, const Schedule &schedule, unsigned processors)
    {
        const size_t n = boost::num_vertices(graph);
        if (schedule.empty() || processors == 0)
            return makeListState(graph, processors);

        LayeredState state;
        const unsigned pcount = std::max(1u, processors);
        state.processors.assign(pcount, {});
        state.proc_of.assign(n, 0);
        state.tier_of.assign(n, 0);

        std::vector<char> placed(n, 0);
        const auto &placement = schedule.placement();

        // Warm-start: preserve processor/timing assignment when placement exists.
        if (!placement.empty())
        {
            struct PlacedItem
            {
                size_t id = 0;
                weight_t start = 0;
                weight_t finish = 0;
            };

            std::vector<std::vector<PlacedItem>> per_proc(pcount);

            for (const auto &job : schedule)
            {
                if (job.id >= n || placed[job.id])
                    continue;
                auto pit = placement.find(job.id);
                if (pit == placement.end())
                    continue;

                unsigned p = pit->second.processor;
                if (p >= pcount)
                    p %= pcount;
                per_proc[p].push_back({job.id, pit->second.start, pit->second.finish});
                placed[job.id] = 1;
            }

            for (const auto &[id, plc] : placement)
            {
                if (id >= n || placed[id])
                    continue;
                unsigned p = plc.processor;
                if (p >= pcount)
                    p %= pcount;
                per_proc[p].push_back({id, plc.start, plc.finish});
                placed[id] = 1;
            }

            for (unsigned p = 0; p < pcount; ++p)
            {
                auto &chain = per_proc[p];
                std::stable_sort(chain.begin(), chain.end(),
                                 [](const PlacedItem &a, const PlacedItem &b)
                                 {
                                     if (a.start != b.start)
                                         return a.start < b.start;
                                     if (a.finish != b.finish)
                                         return a.finish < b.finish;
                                     return a.id < b.id;
                                 });

                for (const auto &item : chain)
                {
                    state.proc_of[item.id] = p;
                    state.tier_of[item.id] = state.processors[p].size();
                    state.processors[p].push_back(item.id);
                }
            }
        }

        std::vector<weight_t> proc_load(state.processors.size(), 0);
        for (unsigned p = 0; p < state.processors.size(); ++p)
        {
            for (const auto task : state.processors[p])
                proc_load[p] += durationOf(graph, task);
        }

        for (const auto &job : schedule)
        {
            if (job.id >= n || placed[job.id])
                continue;

            const auto dur = durationOf(graph, job.id);
            unsigned best_p = 0;
            for (unsigned p = 1; p < state.processors.size(); ++p)
                if (proc_load[p] < proc_load[best_p])
                    best_p = p;

            state.proc_of[job.id] = best_p;
            state.tier_of[job.id] = state.processors[best_p].size();
            state.processors[best_p].push_back(job.id);
            proc_load[best_p] += dur;
            placed[job.id] = 1;
        }

        for (size_t v = 0; v < n; ++v)
        {
            if (placed[v])
                continue;
            unsigned best_p = 0;
            for (unsigned p = 1; p < state.processors.size(); ++p)
                if (proc_load[p] < proc_load[best_p])
                    best_p = p;
            state.proc_of[v] = best_p;
            state.tier_of[v] = state.processors[best_p].size();
            state.processors[best_p].push_back(v);
            proc_load[best_p] += durationOf(graph, v);
        }

        return state;
    }

    LayeredEval evaluateLayeredState(const Graph &graph, const LayeredState &state, weight_t memory_limit)
    {
        const size_t n = boost::num_vertices(graph);
        LayeredEval eval;
        eval.start.assign(n, 0);
        eval.finish.assign(n, 0);

        std::vector<std::vector<size_t>> succ(n);
        std::vector<unsigned> indeg(n, 0);
        std::vector<weight_t> max_pred_finish(n, 0);

        auto add_edge = [&](size_t u, size_t v)
        {
            succ[u].push_back(v);
            indeg[v]++;
        };

        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real)
                continue;
            add_edge(boost::source(e, graph), boost::target(e, graph));
        }

        for (const auto &chain : state.processors)
        {
            for (size_t i = 0; i + 1 < chain.size(); ++i)
                add_edge(chain[i], chain[i + 1]);
        }

        std::queue<size_t> q;
        for (size_t v = 0; v < n; ++v)
            if (indeg[v] == 0)
                q.push(v);

        size_t visited = 0;
        while (!q.empty())
        {
            auto u = q.front();
            q.pop();
            visited++;

            eval.start[u] = max_pred_finish[u];
            eval.finish[u] = eval.start[u] + durationOf(graph, u);
            eval.makespan = std::max(eval.makespan, eval.finish[u]);

            for (const auto v : succ[u])
            {
                max_pred_finish[v] = std::max(max_pred_finish[v], eval.finish[u]);
                if (--indeg[v] == 0)
                    q.push(v);
            }
        }

        if (visited != n)
        {
            eval.acyclic = false;
            eval.overflow = std::numeric_limits<weight_t>::max() / 4;
            return eval;
        }

        struct Event
        {
            weight_t t;
            weight_t delta;
        };
        std::vector<Event> events;
        events.reserve(2 * boost::num_edges(graph));

        auto bindex = additionals::buildBufferIndex(graph);
        for (size_t parent = 0; parent < bindex.groups.size(); ++parent)
        {
            for (const auto &[bid, bg] : bindex.groups[parent])
            {
                (void)bid;
                if (bg.weight <= 0)
                    continue;

                const auto t_alloc = eval.start[parent];
                auto t_free = eval.finish[parent];
                for (const auto c : bg.consumers)
                    if (c < eval.finish.size())
                        t_free = std::max(t_free, eval.finish[c]);
                if (t_free < t_alloc)
                    t_free = t_alloc;

                events.push_back({t_alloc, bg.weight});
                events.push_back({t_free, -bg.weight});
            }
        }

        std::sort(events.begin(), events.end(),
                  [](const Event &a, const Event &b)
                  {
                      if (a.t != b.t)
                          return a.t < b.t;
                      // R(t): [alloc, free), so release at free happens before allocations at same t.
                      return a.delta < b.delta;
                  });

        weight_t used = 0;
        for (const auto &ev : events)
        {
            used += ev.delta;
            if (used < 0)
                used = 0;
            eval.peak_memory = std::max(eval.peak_memory, used);
            if (memory_limit != std::numeric_limits<weight_t>::max() && used > memory_limit)
                eval.overflow = std::max(eval.overflow, used - memory_limit);
        }

        return eval;
    }

    Schedule toSchedule(const Graph &graph, const LayeredState &state, const LayeredEval &eval)
    {
        std::vector<size_t> order(boost::num_vertices(graph));
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(),
                         [&](size_t a, size_t b)
                         {
                             if (eval.start[a] != eval.start[b])
                                 return eval.start[a] < eval.start[b];
                             if (eval.finish[a] != eval.finish[b])
                                 return eval.finish[a] < eval.finish[b];
                             return a < b;
                         });

        Schedule out(order.size(), graph.name());
        auto nums = boost::get(vertex_num_t(), graph);
        for (const auto v : order)
        {
            out.setDisplayId(v, static_cast<size_t>(nums[v]));
            out.push(v, durationOf(graph, v), 0);
            const auto proc = (v < state.proc_of.size()) ? state.proc_of[v] : 0u;
            const auto start = (v < eval.start.size()) ? eval.start[v] : 0;
            const auto finish = (v < eval.finish.size()) ? eval.finish[v] : start;
            out.setPlacement(v, proc, start, finish);
        }
        out.setCost(eval.makespan);
        return out;
    }

    size_t taskTier(const LayeredState &state, size_t task)
    {
        if (task < state.tier_of.size())
            return state.tier_of[task];
        return 0;
    }

    bool moveTaskToProcessor(LayeredState &state, size_t task, unsigned new_proc, size_t target_tier)
    {
        if (task >= state.proc_of.size() || new_proc >= state.processors.size())
            return false;

        const auto old_proc = state.proc_of[task];
        if (old_proc == new_proc)
            return false;

        auto &from = state.processors[old_proc];
        auto fit = std::find(from.begin(), from.end(), task);
        if (fit == from.end())
            return false;
        from.erase(fit);

        auto &to = state.processors[new_proc];
        target_tier = std::min(target_tier, to.size());
        to.insert(to.begin() + static_cast<long long>(target_tier), task);

        rebuildIndices(state);
        return true;
    }

    bool moveTaskToTier(LayeredState &state, size_t task, size_t target_tier)
    {
        if (task >= state.proc_of.size())
            return false;

        const auto p = state.proc_of[task];
        auto &chain = state.processors[p];
        if (chain.size() <= 1)
            return false;

        auto it = std::find(chain.begin(), chain.end(), task);
        if (it == chain.end())
            return false;

        const auto current = static_cast<size_t>(std::distance(chain.begin(), it));
        target_tier = std::min(target_tier, chain.size() - 1);
        if (current == target_tier)
            return false;

        chain.erase(chain.begin() + static_cast<long long>(current));
        chain.insert(chain.begin() + static_cast<long long>(target_tier), task);

        rebuildIndices(state);
        return true;
    }
}
