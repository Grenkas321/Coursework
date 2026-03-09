#include "Greedy.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "BufferIndex.h"

namespace scheduling_problem::algorithms
{
    namespace
    {
        struct ProcSlot
        {
            size_t task = 0;
            weight_t start = 0;
            weight_t finish = 0;
        };

        struct GroupState
        {
            weight_t weight = 0;
            size_t remaining = 0;
            weight_t parent_start = 0;
            weight_t parent_finish = 0;
            weight_t max_consumer_finish = 0;
            bool parent_scheduled = false;
        };

        struct Event
        {
            weight_t time = 0;
            weight_t delta = 0;
        };

        inline bool isUnlimited(weight_t limit)
        {
            return limit == std::numeric_limits<weight_t>::max();
        }

        weight_t durationOf(const Graph &graph, size_t v)
        {
            auto exec = boost::get(vertex_exec_time_t(), graph);
            auto w = boost::get(vertex_weight_t(), graph);
            const auto d = exec[v];
            return d > 0 ? d : std::max<weight_t>(1, w[v]);
        }

        weight_t earliestStartOnProcessor(const std::vector<ProcSlot> &slots,
                                          weight_t dep_ready,
                                          weight_t duration)
        {
            weight_t t = dep_ready;
            for (const auto &slot : slots)
            {
                if (t + duration <= slot.start)
                    return t; // Fits in a hole.
                if (t < slot.finish)
                    t = slot.finish;
            }
            return t;
        }

        void appendBaseMemoryEvents(const std::vector<std::unordered_map<int, GroupState>> &groups,
                                    std::vector<Event> &events)
        {
            for (const auto &by_bid : groups)
            {
                for (const auto &[bid, gs] : by_bid)
                {
                    (void)bid;
                    if (!gs.parent_scheduled || gs.weight <= 0)
                        continue;

                    events.push_back({gs.parent_start, gs.weight});
                    if (gs.remaining == 0)
                    {
                        const auto free_time = std::max(gs.parent_finish, gs.max_consumer_finish);
                        events.push_back({free_time, -gs.weight});
                    }
                }
            }
        }

        bool feasibleUnderMemory(weight_t memory_limit,
                                 const std::vector<std::unordered_map<int, GroupState>> &groups,
                                 size_t candidate,
                                 weight_t cand_start,
                                 weight_t cand_finish,
                                 const std::vector<std::vector<std::pair<size_t, int>>> &incoming_groups)
        {
            if (isUnlimited(memory_limit))
                return true;

            std::vector<Event> events;
            events.reserve(2 * groups.size() + 16);
            appendBaseMemoryEvents(groups, events);

            // New allocations produced by candidate buffers.
            if (candidate < groups.size())
            {
                for (const auto &[bid, gs] : groups[candidate])
                {
                    (void)bid;
                    if (gs.weight > 0)
                        events.push_back({cand_start, gs.weight});
                }
            }

            // Releases caused by closing parent buffers on this candidate.
            if (candidate < incoming_groups.size())
            {
                for (const auto &[parent, bid] : incoming_groups[candidate])
                {
                    if (parent >= groups.size())
                        continue;
                    auto it = groups[parent].find(bid);
                    if (it == groups[parent].end())
                        continue;
                    const auto &gs = it->second;
                    if (!gs.parent_scheduled || gs.weight <= 0 || gs.remaining == 0)
                        continue;
                    if (gs.remaining == 1)
                    {
                        const auto free_time = std::max(gs.parent_finish,
                                                        std::max(gs.max_consumer_finish, cand_finish));
                        events.push_back({free_time, -gs.weight});
                    }
                }
            }

            std::sort(events.begin(), events.end(),
                      [](const Event &a, const Event &b)
                      {
                          if (a.time != b.time)
                              return a.time < b.time;
                          return a.delta < b.delta; // Release before allocate on same timestamp.
                      });

            weight_t used = 0;
            for (const auto &ev : events)
            {
                used += ev.delta;
                if (used < 0)
                    used = 0;
                if (used > memory_limit)
                    return false;
            }
            return true;
        }

        weight_t minRequiredMemory(const additionals::BufferIndex &bindex)
        {
            weight_t need = 0;
            for (const auto &by_bid : bindex.groups)
            {
                for (const auto &[bid, info] : by_bid)
                {
                    (void)bid;
                    need = std::max(need, info.weight);
                }
            }
            return need;
        }
    }

    Greedy::Greedy(const std::string &label)
        : BaseOptimization(label)
    {
    }

    std::unique_ptr<BaseOptimization> Greedy::copy() const
    {
        return std::unique_ptr<BaseOptimization>(new Greedy(*this));
    }

    void Greedy::setParams(const ParamSet &params)
    {
        for (const auto &[name, val] : params)
        {
            if (name == "processors")
            {
                processors_ = std::max(1u, (unsigned)val);
            }
            else if (name == "memory_limit")
            {
                const auto raw = (double)val;
                const auto inf_like = static_cast<double>(std::numeric_limits<weight_t>::max() / 2);
                if (raw >= inf_like)
                    memory_limit_ = std::numeric_limits<weight_t>::max();
                else
                    memory_limit_ = static_cast<weight_t>(raw);
            }
            else
            {
                BaseOptimization::setParams({{name, val}});
            }
        }
    }

    ParamSet Greedy::getParams() const
    {
        auto params = BaseOptimization::getParams();
        params["processors"] = processors_;
        params["memory_limit"] = static_cast<double>(memory_limit_);
        return params;
    }

    Schedule Greedy::schedule_(const Graph &graph)
    {
        const size_t n = boost::num_vertices(graph);
        if (n == 0)
            return Schedule(0, graph.name());

        const unsigned pcount = std::max(1u, processors_);

        std::vector<std::vector<size_t>> parents(n), children(n);
        std::vector<size_t> remaining_parents(n, 0);
        std::vector<weight_t> duration(n, 1);
        for (size_t v = 0; v < n; ++v)
            duration[v] = durationOf(graph, v);

        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real)
                continue;
            const auto u = static_cast<size_t>(boost::source(e, graph));
            const auto v = static_cast<size_t>(boost::target(e, graph));
            children[u].push_back(v);
            parents[v].push_back(u);
            remaining_parents[v]++;
        }

        const auto bindex = additionals::buildBufferIndex(graph);
        const auto min_required = minRequiredMemory(bindex);
        if (!isUnlimited(memory_limit_) && memory_limit_ < min_required)
        {
            throw std::invalid_argument(
                "Memory limit " + std::to_string(memory_limit_) +
                " is too small for graph '" + graph.name() +
                "'. Minimal required value is " + std::to_string(min_required) + ".");
        }

        std::vector<std::unordered_map<int, GroupState>> groups(n);
        for (size_t parent = 0; parent < bindex.groups.size(); ++parent)
        {
            for (const auto &[bid, info] : bindex.groups[parent])
            {
                GroupState gs;
                gs.weight = info.weight;
                gs.remaining = info.consumers.size();
                groups[parent][bid] = gs;
            }
        }

        std::vector<std::vector<std::pair<size_t, int>>> incoming_groups(n);
        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real)
                continue;
            const auto u = static_cast<size_t>(boost::source(e, graph));
            const auto v = static_cast<size_t>(boost::target(e, graph));
            const int bid = boost::get(edge_buffer_id_t(), graph, e);
            incoming_groups[v].push_back({u, bid});
        }
        for (auto &lst : incoming_groups)
        {
            std::sort(lst.begin(), lst.end());
            lst.erase(std::unique(lst.begin(), lst.end()), lst.end());
        }

        std::vector<std::vector<ProcSlot>> proc_slots(pcount);
        std::vector<char> scheduled(n, 0);
        std::vector<weight_t> start_time(n, 0), finish_time(n, 0);
        std::vector<unsigned> proc_of(n, 0);

        std::set<size_t> ready;
        for (size_t v = 0; v < n; ++v)
            if (remaining_parents[v] == 0)
                ready.insert(v);

        size_t done = 0;
        while (done < n)
        {
            if (ready.empty())
                throw std::runtime_error("Cannot continue greedy scheduling: graph is not a DAG.");

            bool found = false;
            size_t best_task = 0;
            unsigned best_proc = 0;
            weight_t best_start = 0;
            weight_t best_finish = 0;

            for (const auto task : ready)
            {
                weight_t dep_ready = 0;
                for (const auto p : parents[task])
                    dep_ready = std::max(dep_ready, finish_time[p]);

                for (unsigned p = 0; p < pcount; ++p)
                {
                    const auto start = earliestStartOnProcessor(proc_slots[p], dep_ready, duration[task]);
                    const auto finish = start + duration[task];

                    if (!feasibleUnderMemory(memory_limit_, groups, task, start, finish, incoming_groups))
                        continue;

                    if (!found ||
                        start < best_start ||
                        (start == best_start && (task < best_task ||
                                                 (task == best_task && p < best_proc))))
                    {
                        found = true;
                        best_task = task;
                        best_proc = p;
                        best_start = start;
                        best_finish = finish;
                    }
                }
            }

            if (!found)
            {
                throw std::runtime_error(
                    "No feasible greedy placement under memory limit " +
                    std::to_string(memory_limit_) +
                    " for graph '" + graph.name() + "'.");
            }

            // Commit task placement.
            auto &slots = proc_slots[best_proc];
            ProcSlot slot{best_task, best_start, best_finish};
            auto it = std::lower_bound(
                slots.begin(), slots.end(), slot,
                [](const ProcSlot &a, const ProcSlot &b)
                {
                    if (a.start != b.start)
                        return a.start < b.start;
                    if (a.finish != b.finish)
                        return a.finish < b.finish;
                    return a.task < b.task;
                });
            slots.insert(it, slot);

            scheduled[best_task] = 1;
            start_time[best_task] = best_start;
            finish_time[best_task] = best_finish;
            proc_of[best_task] = best_proc;
            done++;

            // Parent-side buffer allocations.
            for (auto &[bid, gs] : groups[best_task])
            {
                (void)bid;
                gs.parent_scheduled = true;
                gs.parent_start = best_start;
                gs.parent_finish = best_finish;
            }

            // Child-side potential closures.
            for (const auto &[parent, bid] : incoming_groups[best_task])
            {
                if (parent >= groups.size())
                    continue;
                auto git = groups[parent].find(bid);
                if (git == groups[parent].end())
                    continue;
                auto &gs = git->second;
                if (gs.remaining > 0)
                {
                    gs.remaining--;
                    gs.max_consumer_finish = std::max(gs.max_consumer_finish, best_finish);
                }
            }

            ready.erase(best_task);
            for (const auto child : children[best_task])
            {
                if (remaining_parents[child] > 0)
                {
                    remaining_parents[child]--;
                    if (remaining_parents[child] == 0)
                        ready.insert(child);
                }
            }
        }

        std::vector<size_t> order(n);
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(),
                         [&](size_t a, size_t b)
                         {
                             if (start_time[a] != start_time[b])
                                 return start_time[a] < start_time[b];
                             if (finish_time[a] != finish_time[b])
                                 return finish_time[a] < finish_time[b];
                             return a < b;
                         });

        Schedule out(n, graph.name());
        weight_t makespan = 0;
        for (const auto v : order)
        {
            out.push(v, duration[v], 0);
            out.setPlacement(v, proc_of[v], start_time[v], finish_time[v]);
            makespan = std::max(makespan, finish_time[v]);
        }
        out.setCost(makespan);
        return out;
    }

    size_t Greedy::choice(const Graph &graph,
                          const ScheduleStatus &schedule,
                          size_t curr_vid)
    {
        auto heu = heuInfo(graph, schedule, curr_vid);
        return std::max_element(heu.begin(), heu.end(),
                                [](const auto &a, const auto &b)
                                { return a.second < b.second; })
            ->first;
    }

    std::unordered_map<size_t, double>
    Greedy::heuInfo(const Graph &graph, const ScheduleStatus &status, size_t curr_vid)
    {
        const size_t lower = status.lower(curr_vid, graph);
        const size_t pos_count = status.size() + 1 - lower;

        std::unordered_map<size_t, double> out;
        out.reserve(pos_count);

        if (status.size() == 0)
        {
            out[0] = 1.0;
            return out;
        }

        std::vector<weight_t> targets(pos_count);
        weight_t stable_cost = 0, stable_target = 0;

        for (size_t pos = 0; pos < lower; ++pos)
        {
            stable_target += status[pos].volume;
            if (stable_target > stable_cost)
                stable_cost = stable_target;
            stable_target -= status[pos].release;
        }

        weight_t target = 0;
        const weight_t w_curr = boost::get(vertex_weight_t(), graph, curr_vid);
        for (size_t pos = 0; pos < status.size(); ++pos)
        {
            target += status[pos].volume;
            if (pos >= lower)
                targets[pos - lower] = target;
            target -= status[pos].release;
        }
        targets[pos_count - 1] = target + w_curr;

        weight_t base_max = std::max(stable_cost, *std::max_element(targets.begin(), targets.end()));
        out[status.size()] = 1.0 / std::max<weight_t>(1, base_max);

        size_t last_child = 0, child_remain = 0;
        weight_t curr_release = boost::out_degree(curr_vid, graph) ? 0 : w_curr;
        std::vector<std::pair<size_t, weight_t>> released;
        for (auto parent : status.parents(curr_vid, graph))
        {
            std::tie(child_remain, last_child) = status.releaseOn(parent, graph);
            if (child_remain == 1)
            {
                const auto pw = boost::get(vertex_weight_t(), graph, parent);
                released.emplace_back(last_child, pw);
                curr_release += pw;
            }
        }
        std::sort(released.begin(), released.end(),
                  [](const auto &a, const auto &b)
                  { return a.first < b.first; });

        weight_t right_max = 0, left_max = 0;
        for (size_t curr_pos = status.size(); curr_pos-- > lower;)
        {
            for (const auto &rel : released)
            {
                if (rel.first == curr_pos)
                    curr_release -= rel.second;
                else if (rel.first > curr_pos)
                    break;
            }
            const size_t idx = curr_pos - lower;
            const size_t next = idx + 1;

            targets[idx] = targets[idx] - status[curr_pos].volume + w_curr;
            targets[next] = targets[idx] - curr_release + status[curr_pos].volume;

            if (targets[next] > right_max)
                right_max = targets[next];
            left_max = *std::max_element(targets.begin(), targets.begin() + next);

            const weight_t cost_max = std::max(left_max, right_max);
            out[curr_pos] = 1.0 / std::max<weight_t>(1, cost_max);
        }

        return out;
    }
}
