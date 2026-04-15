#include "MemoryFeasibleTiming.h"

#include <algorithm>
#include <limits>

namespace scheduling_problem::algorithms
{
    namespace
    {
        bool eventLess(const MemoryEvent &lhs, const MemoryEvent &rhs)
        {
            if (lhs.time != rhs.time)
                return lhs.time < rhs.time;
            return lhs.delta < rhs.delta;
        }
    }

    bool isUnlimitedMemory(weight_t limit)
    {
        return limit == std::numeric_limits<weight_t>::max();
    }

    std::vector<std::unordered_map<int, MemoryGroupState>>
    makeMemoryGroups(const additionals::BufferIndex &bindex)
    {
        std::vector<std::unordered_map<int, MemoryGroupState>> groups(bindex.groups.size());
        for (size_t parent = 0; parent < bindex.groups.size(); ++parent)
        {
            for (const auto &[bid, info] : bindex.groups[parent])
            {
                MemoryGroupState gs;
                gs.weight = info.weight;
                gs.remaining = info.consumers.size();
                groups[parent][bid] = gs;
            }
        }
        return groups;
    }

    std::vector<std::vector<std::pair<size_t, int>>>
    buildIncomingGroups(const Graph &graph)
    {
        const size_t n = boost::num_vertices(graph);
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
        return incoming_groups;
    }

    weight_t totalProducedWeight(const additionals::BufferIndex &bindex)
    {
        weight_t total = 0;
        for (const auto &by_bid : bindex.groups)
            for (const auto &[bid, info] : by_bid)
            {
                (void)bid;
                if (info.weight > 0)
                    total += info.weight;
            }
        return total;
    }

    MemoryBaseContext buildMemoryBaseContext(
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups)
    {
        MemoryBaseContext ctx;
        ctx.change_times.push_back(0);

        for (const auto &by_bid : groups)
        {
            for (const auto &[bid, gs] : by_bid)
            {
                (void)bid;
                if (!gs.parent_scheduled || gs.weight <= 0)
                    continue;

                ctx.produced_upper += gs.weight;
                ctx.events.push_back({gs.parent_start, gs.weight});
                ctx.change_times.push_back(gs.parent_start);
                if (gs.remaining == 0)
                {
                    const auto free_time = std::max(gs.parent_finish, gs.max_consumer_finish);
                    ctx.events.push_back({free_time, -gs.weight});
                    ctx.change_times.push_back(free_time);
                }
            }
        }

        std::sort(ctx.events.begin(), ctx.events.end(), eventLess);
        std::sort(ctx.change_times.begin(), ctx.change_times.end());
        ctx.change_times.erase(std::unique(ctx.change_times.begin(), ctx.change_times.end()),
                               ctx.change_times.end());
        return ctx;
    }

    bool feasibleUnderMemory(
        weight_t memory_limit,
        const MemoryBaseContext &base,
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups,
        size_t candidate,
        weight_t cand_start,
        weight_t cand_finish,
        const std::vector<std::vector<std::pair<size_t, int>>> &incoming_groups)
    {
        if (isUnlimitedMemory(memory_limit))
            return true;

        weight_t produced_upper = base.produced_upper;
        if (candidate < groups.size())
        {
            for (const auto &[bid, gs] : groups[candidate])
            {
                (void)bid;
                if (gs.weight > 0)
                    produced_upper += gs.weight;
            }
        }
        if (produced_upper <= memory_limit)
            return true;

        std::vector<MemoryEvent> candidate_events;
        candidate_events.reserve(8);

        if (candidate < groups.size())
        {
            for (const auto &[bid, gs] : groups[candidate])
            {
                (void)bid;
                if (gs.weight > 0)
                    candidate_events.push_back({cand_start, gs.weight});
            }
        }

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
                    const auto free_time =
                        std::max(gs.parent_finish, std::max(gs.max_consumer_finish, cand_finish));
                    candidate_events.push_back({free_time, -gs.weight});
                }
            }
        }

        std::sort(candidate_events.begin(), candidate_events.end(), eventLess);

        weight_t used = 0;
        size_t i = 0;
        size_t j = 0;
        while (i < base.events.size() || j < candidate_events.size())
        {
            MemoryEvent ev;
            if (j >= candidate_events.size() ||
                (i < base.events.size() && !eventLess(candidate_events[j], base.events[i])))
            {
                ev = base.events[i++];
            }
            else
            {
                ev = candidate_events[j++];
            }

            used += ev.delta;
            if (used < 0)
                used = 0;
            if (used > memory_limit)
                return false;
        }
        return true;
    }

    bool earliestFeasibleStart(
        weight_t dep_ready,
        weight_t duration,
        weight_t memory_limit,
        const MemoryBaseContext &base,
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups,
        size_t candidate,
        const std::vector<std::vector<std::pair<size_t, int>>> &incoming_groups,
        weight_t &start_out)
    {
        std::vector<weight_t> candidates;
        candidates.reserve(1 + 2 * base.change_times.size());
        candidates.push_back(dep_ready);

        for (const auto t : base.change_times)
        {
            if (t >= dep_ready)
                candidates.push_back(t);

            if (t >= duration)
            {
                const auto aligned = t - duration;
                if (aligned >= dep_ready)
                    candidates.push_back(aligned);
            }
        }

        std::sort(candidates.begin(), candidates.end());
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

        for (const auto start : candidates)
        {
            const auto finish = start + duration;
            if (feasibleUnderMemory(memory_limit,
                                    base,
                                    groups,
                                    candidate,
                                    start,
                                    finish,
                                    incoming_groups))
            {
                start_out = start;
                return true;
            }
        }

        return false;
    }
}
