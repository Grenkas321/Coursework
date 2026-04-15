#include "SimulatedAnnealing.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <string>

#include "LayeredSchedule.h"
#include "TopologicalSort.h"

namespace
{
    static unsigned mixSeed(unsigned base, unsigned salt = 0u)
    {
        uint64_t x = static_cast<uint64_t>(base) ^ 0x9e3779b97f4a7c15ULL;
        x ^= static_cast<uint64_t>(salt) + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2);
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return static_cast<unsigned>(x);
    }

    inline bool isUnlimited(scheduling_problem::weight_t limit)
    {
        return limit == std::numeric_limits<scheduling_problem::weight_t>::max();
    }

    scheduling_problem::weight_t clampObjectiveTrace(double value)
    {
        using scheduling_problem::weight_t;
        const auto hi = static_cast<double>(std::numeric_limits<weight_t>::max());
        const auto lo = static_cast<double>(std::numeric_limits<weight_t>::min());

        if (!std::isfinite(value) || value >= hi)
            return std::numeric_limits<weight_t>::max();
        if (value <= lo)
            return std::numeric_limits<weight_t>::min();
        return static_cast<weight_t>(std::llround(value));
    }

    scheduling_problem::weight_t durationOf(const scheduling_problem::Graph &graph, size_t v)
    {
        auto exec = boost::get(scheduling_problem::vertex_exec_time_t(), graph);
        auto w = boost::get(scheduling_problem::vertex_weight_t(), graph);
        const auto d = exec[v];
        return d > 0 ? d : std::max<scheduling_problem::weight_t>(1, w[v]);
    }

    std::vector<scheduling_problem::weight_t>
    computeBottomLevels(const scheduling_problem::Graph &graph)
    {
        const size_t n = boost::num_vertices(graph);
        std::vector<scheduling_problem::weight_t> rank(n, 0);
        auto topo = scheduling_problem::topo_sort(graph);
        if (topo.size() != n)
        {
            topo.resize(n);
            std::iota(topo.begin(), topo.end(), 0);
        }

        for (auto it = topo.rbegin(); it != topo.rend(); ++it)
        {
            const auto v = static_cast<size_t>(*it);
            auto best = durationOf(graph, v);
            for (auto e : boost::make_iterator_range(boost::out_edges(v, graph)))
            {
                if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                    continue;
                const auto child = static_cast<size_t>(boost::target(e, graph));
                best = std::max(best, durationOf(graph, v) + rank[child]);
            }
            rank[v] = best;
        }

        return rank;
    }

    std::vector<size_t>
    makePriorityOrder(const std::vector<scheduling_problem::weight_t> &priority)
    {
        std::vector<size_t> order(priority.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(),
                         [&](size_t a, size_t b)
                         {
                             if (priority[a] != priority[b])
                                 return priority[a] > priority[b];
                             return a < b;
                         });
        return order;
    }

    scheduling_problem::algorithms::LayeredState
    makePriorityState(const scheduling_problem::Graph &graph,
                      unsigned processors,
                      const std::vector<scheduling_problem::weight_t> &priority)
    {
        const size_t n = boost::num_vertices(graph);
        const unsigned pcount = std::max(1u, processors);

        scheduling_problem::algorithms::LayeredState state;
        state.processors.assign(pcount, {});
        state.proc_of.assign(n, 0);
        state.tier_of.assign(n, 0);

        std::vector<std::vector<size_t>> parents(n), children(n);
        std::vector<size_t> remaining_parents(n, 0);
        std::vector<scheduling_problem::weight_t> proc_free(pcount, 0);
        std::vector<scheduling_problem::weight_t> finish(n, 0);

        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                continue;
            const auto u = static_cast<size_t>(boost::source(e, graph));
            const auto v = static_cast<size_t>(boost::target(e, graph));
            children[u].push_back(v);
            parents[v].push_back(u);
            remaining_parents[v]++;
        }

        std::vector<size_t> ready;
        ready.reserve(n);
        for (size_t v = 0; v < n; ++v)
            if (remaining_parents[v] == 0)
                ready.push_back(v);

        while (!ready.empty())
        {
            auto best_it = ready.begin();
            for (auto it = ready.begin() + 1; it != ready.end(); ++it)
            {
                const auto lhs = *it;
                const auto rhs = *best_it;
                if (priority[lhs] > priority[rhs] ||
                    (priority[lhs] == priority[rhs] && lhs < rhs))
                {
                    best_it = it;
                }
            }

            const auto task = *best_it;
            ready.erase(best_it);

            scheduling_problem::weight_t dep_ready = 0;
            for (const auto parent : parents[task])
                dep_ready = std::max(dep_ready, finish[parent]);

            unsigned best_proc = 0;
            auto best_start = std::max(dep_ready, proc_free[0]);
            auto best_finish = best_start + durationOf(graph, task);
            for (unsigned p = 1; p < pcount; ++p)
            {
                const auto start = std::max(dep_ready, proc_free[p]);
                const auto finish_time = start + durationOf(graph, task);
                if (finish_time < best_finish ||
                    (finish_time == best_finish &&
                     (start < best_start || (start == best_start && p < best_proc))))
                {
                    best_proc = p;
                    best_start = start;
                    best_finish = finish_time;
                }
            }

            state.proc_of[task] = best_proc;
            state.tier_of[task] = state.processors[best_proc].size();
            state.processors[best_proc].push_back(task);
            proc_free[best_proc] = best_finish;
            finish[task] = best_finish;

            for (const auto child : children[task])
            {
                if (--remaining_parents[child] == 0)
                    ready.push_back(child);
            }
        }

        return state;
    }

    bool betterEval(const scheduling_problem::algorithms::LayeredEval &lhs,
                    const scheduling_problem::algorithms::LayeredEval &rhs,
                    double overflow_penalty)
    {
        if (lhs.feasible() != rhs.feasible())
            return lhs.feasible();
        return lhs.score(overflow_penalty) < rhs.score(overflow_penalty);
    }

    bool applyGuidedPromotion(scheduling_problem::algorithms::LayeredState &state,
                              const std::vector<size_t> &priority_order,
                              std::mt19937 &rng)
    {
        std::vector<size_t> candidates;
        const size_t limit = std::min<size_t>(priority_order.size(), 96);
        candidates.reserve(limit);

        for (size_t i = 0; i < limit; ++i)
        {
            const auto task = priority_order[i];
            if (task < state.tier_of.size() && state.tier_of[task] > 0)
                candidates.push_back(task);
        }

        if (candidates.empty())
            return false;

        std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
        const auto task = candidates[pick(rng)];
        const auto current_tier = state.tier_of[task];
        if (current_tier == 0)
            return false;

        std::bernoulli_distribution to_front(0.75);
        size_t target_tier = 0;
        if (!to_front(rng) && current_tier > 1)
        {
            std::uniform_int_distribution<size_t> tier_dist(0, current_tier - 1);
            target_tier = tier_dist(rng);
        }

        return scheduling_problem::algorithms::moveTaskToTier(state, task, target_tier);
    }

    bool applyGuidedMigration(scheduling_problem::algorithms::LayeredState &state,
                              const std::vector<size_t> &priority_order,
                              std::mt19937 &rng)
    {
        if (state.processors.size() <= 1)
            return false;

        std::vector<size_t> candidates;
        const size_t limit = std::min<size_t>(priority_order.size(), 96);
        candidates.reserve(limit);
        for (size_t i = 0; i < limit; ++i)
            candidates.push_back(priority_order[i]);

        if (candidates.empty())
            return false;

        std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
        const auto task = candidates[pick(rng)];
        if (task >= state.proc_of.size())
            return false;

        const auto old_proc = state.proc_of[task];
        std::vector<unsigned> proc_order(state.processors.size());
        std::iota(proc_order.begin(), proc_order.end(), 0u);
        std::stable_sort(proc_order.begin(), proc_order.end(),
                         [&](unsigned a, unsigned b)
                         {
                             if (a == old_proc) return false;
                             if (b == old_proc) return true;
                             if (state.processors[a].size() != state.processors[b].size())
                                 return state.processors[a].size() < state.processors[b].size();
                             return a < b;
                         });

        for (const auto new_proc : proc_order)
        {
            if (new_proc == old_proc)
                continue;

            const auto max_front = std::min<size_t>(state.processors[new_proc].size(), 2);
            std::uniform_int_distribution<size_t> tier_dist(0, max_front);
            if (scheduling_problem::algorithms::moveTaskToProcessor(state, task, new_proc, tier_dist(rng)))
                return true;
        }

        return false;
    }

    bool applyRandomLayeredMove(scheduling_problem::algorithms::LayeredState &state,
                                const std::vector<size_t> &priority_order,
                                std::mt19937 &rng)
    {
        const size_t n = state.proc_of.size();
        if (n == 0 || state.processors.empty())
            return false;

        std::uniform_int_distribution<size_t> task_dist(0, n - 1);
        std::uniform_int_distribution<int> move_family(0, 99);
        std::bernoulli_distribution op_kind(0.5); // true: O1(move proc), false: O2(move tier)

        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
            const auto family = move_family(rng);
            if (family < 35 && applyGuidedPromotion(state, priority_order, rng))
                return true;
            if (family >= 35 && family < 60 && applyGuidedMigration(state, priority_order, rng))
                return true;

            const auto task = task_dist(rng);
            if (task >= state.proc_of.size())
                continue;

            if (op_kind(rng) && state.processors.size() > 1)
            {
                std::uniform_int_distribution<unsigned> proc_dist(
                    0, static_cast<unsigned>(state.processors.size() - 1));
                const auto new_proc = proc_dist(rng);
                if (new_proc == state.proc_of[task])
                    continue;

                const auto max_tier = state.processors[new_proc].size();
                std::uniform_int_distribution<size_t> tier_dist(0, max_tier);
                const auto tier = tier_dist(rng);
                if (scheduling_problem::algorithms::moveTaskToProcessor(state, task, new_proc, tier))
                    return true;
            }
            else
            {
                const auto p = state.proc_of[task];
                if (p >= state.processors.size() || state.processors[p].size() <= 1)
                    continue;

                std::uniform_int_distribution<size_t> tier_dist(0, state.processors[p].size() - 1);
                const auto tier = tier_dist(rng);
                if (scheduling_problem::algorithms::moveTaskToTier(state, task, tier))
                    return true;
            }
        }

        return false;
    }

    size_t earliestLegalTierOnProcessor(const scheduling_problem::algorithms::LayeredState &state,
                                        const scheduling_problem::Graph &graph,
                                        size_t task,
                                        unsigned proc)
    {
        size_t earliest = 0;
        for (auto e : boost::make_iterator_range(boost::in_edges(task, graph)))
        {
            if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                continue;
            const auto parent = static_cast<size_t>(boost::source(e, graph));
            if (parent >= state.proc_of.size() || state.proc_of[parent] != proc)
                continue;
            earliest = std::max(earliest, state.tier_of[parent] + 1);
        }
        return earliest;
    }

    size_t latestLegalInsertionTier(const scheduling_problem::algorithms::LayeredState &state,
                                    const scheduling_problem::Graph &graph,
                                    size_t task,
                                    unsigned proc)
    {
        size_t latest = state.processors[proc].size();
        for (auto e : boost::make_iterator_range(boost::out_edges(task, graph)))
        {
            if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                continue;
            const auto child = static_cast<size_t>(boost::target(e, graph));
            if (child >= state.proc_of.size() || state.proc_of[child] != proc)
                continue;
            latest = std::min(latest, state.tier_of[child]);
        }
        return latest;
    }

    scheduling_problem::weight_t computeInternalIdle(const scheduling_problem::algorithms::LayeredState &state,
                                                     const scheduling_problem::algorithms::LayeredEval &eval)
    {
        scheduling_problem::weight_t idle = 0;
        for (const auto &chain : state.processors)
        {
            scheduling_problem::weight_t prev_finish = 0;
            for (const auto task : chain)
            {
                if (task >= eval.start.size())
                    continue;
                if (eval.start[task] > prev_finish)
                    idle += eval.start[task] - prev_finish;
                prev_finish = eval.finish[task];
            }
        }
        return idle;
    }

    struct FedorenkoProposalStats
    {
        size_t attempted_trials = 0;
        size_t successful_trials = 0;
        bool best_candidate_used_fedorenko = false;
    };

    struct FedorenkoRepairStats
    {
        size_t attempts = 0;
        size_t improvements = 0;
    };

    bool applyFedorenkoIdleReduction(scheduling_problem::algorithms::LayeredState &state,
                                     const scheduling_problem::algorithms::LayeredEval &eval,
                                     const scheduling_problem::Graph &graph,
                                     scheduling_problem::weight_t memory_limit,
                                     std::mt19937 &rng)
    {
        struct IdleCandidate
        {
            size_t delayed_task = 0;
            unsigned processor = 0;
            scheduling_problem::weight_t idle = 0;
            scheduling_problem::weight_t gap_start = 0;
            scheduling_problem::weight_t gap_end = 0;
            size_t insert_tier = 0;
            size_t blocker = 0;
            scheduling_problem::weight_t blocker_finish = 0;
        };

        std::vector<IdleCandidate> candidates;
        candidates.reserve(state.proc_of.size());

        const size_t n = boost::num_vertices(graph);
        std::vector<scheduling_problem::weight_t> parent_ready(n, 0);
        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                continue;
            const auto parent = static_cast<size_t>(boost::source(e, graph));
            const auto child = static_cast<size_t>(boost::target(e, graph));
            if (parent < eval.finish.size() && child < parent_ready.size())
                parent_ready[child] = std::max(parent_ready[child], eval.finish[parent]);
        }

        for (unsigned p = 0; p < state.processors.size(); ++p)
        {
            scheduling_problem::weight_t prev_finish = 0;
            for (const auto task : state.processors[p])
            {
                if (task >= n)
                    continue;

                const auto start = eval.start[task];
                if (start <= prev_finish)
                {
                    prev_finish = eval.finish[task];
                    continue;
                }

                size_t blocker = n;
                scheduling_problem::weight_t blocker_finish = 0;
                for (auto e : boost::make_iterator_range(boost::in_edges(task, graph)))
                {
                    if (boost::get(scheduling_problem::edge_kind_t(), graph, e) != scheduling_problem::EdgeKind::Real)
                        continue;
                    const auto parent = static_cast<size_t>(boost::source(e, graph));
                    if (eval.finish[parent] > blocker_finish)
                    {
                        blocker_finish = eval.finish[parent];
                        blocker = parent;
                    }
                }

                if (blocker == n || blocker_finish != start || blocker_finish <= prev_finish)
                {
                    prev_finish = eval.finish[task];
                    continue;
                }

                candidates.push_back({
                    task,
                    p,
                    start - prev_finish,
                    prev_finish,
                    start,
                    state.tier_of[task],
                    blocker,
                    blocker_finish,
                });
                prev_finish = eval.finish[task];
            }
        }

        if (candidates.empty())
            return false;

        const auto current_idle = computeInternalIdle(state, eval);
        scheduling_problem::algorithms::LayeredState best_gap_state;
        scheduling_problem::algorithms::LayeredEval best_gap_eval;
        scheduling_problem::weight_t best_gap_idle = current_idle;
        bool gap_fill_found = false;

        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const IdleCandidate &lhs, const IdleCandidate &rhs)
                         {
                             if (lhs.idle != rhs.idle)
                                return lhs.idle > rhs.idle;
                             if (lhs.gap_start != rhs.gap_start)
                                 return lhs.gap_start < rhs.gap_start;
                             if (lhs.blocker_finish != rhs.blocker_finish)
                                 return lhs.blocker_finish > rhs.blocker_finish;
                             if (lhs.delayed_task != rhs.delayed_task)
                                 return lhs.delayed_task < rhs.delayed_task;
                             return lhs.blocker < rhs.blocker;
                         });

        const auto shortlist = std::min<size_t>(candidates.size(), 8);
        for (size_t gap_idx = 0; gap_idx < shortlist; ++gap_idx)
        {
            const auto &gap = candidates[gap_idx];
            const auto gap_len = gap.idle;
            if (gap_len <= 0)
                continue;

            struct FillTask
            {
                size_t task = 0;
                scheduling_problem::weight_t duration = 0;
                scheduling_problem::weight_t pull_left = 0;
                scheduling_problem::weight_t tail_after_gap = 0;
                bool same_proc = false;
            };

            std::vector<FillTask> fill_tasks;
            fill_tasks.reserve(n);

            for (size_t task = 0; task < n; ++task)
            {
                if (task == gap.delayed_task || task >= eval.start.size() || task >= eval.finish.size())
                    continue;

                const auto dur = eval.finish[task] - eval.start[task];
                if (dur <= 0 || dur > gap_len)
                    continue;
                if (parent_ready[task] > gap.gap_start)
                    continue;
                if (eval.start[task] <= gap.gap_start)
                    continue;

                const bool same_proc = task < state.proc_of.size() && state.proc_of[task] == gap.processor;
                if (same_proc && task < state.tier_of.size() && state.tier_of[task] <= gap.insert_tier)
                    continue;

                fill_tasks.push_back({
                    task,
                    dur,
                    eval.start[task] - gap.gap_start,
                    std::max<scheduling_problem::weight_t>(0, eval.finish[task] - gap.gap_end),
                    same_proc,
                });
            }

            std::stable_sort(fill_tasks.begin(), fill_tasks.end(),
                             [](const FillTask &lhs, const FillTask &rhs)
                             {
                                 if (lhs.tail_after_gap != rhs.tail_after_gap)
                                     return lhs.tail_after_gap > rhs.tail_after_gap;
                                 if (lhs.pull_left != rhs.pull_left)
                                     return lhs.pull_left > rhs.pull_left;
                                 if (lhs.duration != rhs.duration)
                                     return lhs.duration > rhs.duration;
                                 return lhs.task < rhs.task;
                             });

            const auto task_shortlist = std::min<size_t>(fill_tasks.size(), 12);
            for (size_t idx = 0; idx < task_shortlist; ++idx)
            {
                const auto &pick_task = fill_tasks[idx];
                auto candidate = state;
                bool moved = false;

                if (pick_task.same_proc)
                {
                    const auto earliest =
                        earliestLegalTierOnProcessor(candidate, graph, pick_task.task, gap.processor);
                    const auto latest =
                        latestLegalInsertionTier(candidate, graph, pick_task.task, gap.processor);
                    if (earliest <= gap.insert_tier &&
                        gap.insert_tier <= latest &&
                        pick_task.task < state.tier_of.size() &&
                        state.tier_of[pick_task.task] > gap.insert_tier)
                    {
                        moved = scheduling_problem::algorithms::moveTaskToTier(candidate,
                                                                               pick_task.task,
                                                                               gap.insert_tier);
                    }
                }
                else
                {
                    const auto earliest =
                        earliestLegalTierOnProcessor(candidate, graph, pick_task.task, gap.processor);
                    const auto latest =
                        latestLegalInsertionTier(candidate, graph, pick_task.task, gap.processor);
                    if (earliest <= gap.insert_tier && gap.insert_tier <= latest)
                    {
                        moved = scheduling_problem::algorithms::moveTaskToProcessor(candidate,
                                                                                    pick_task.task,
                                                                                    gap.processor,
                                                                                    gap.insert_tier);
                    }
                }

                if (!moved)
                    continue;

                auto candidate_eval =
                    scheduling_problem::algorithms::evaluateLayeredState(graph, candidate, memory_limit);
                if (!candidate_eval.feasible())
                    continue;

                const auto candidate_idle = computeInternalIdle(candidate, candidate_eval);
                const bool improves =
                    (candidate_eval.makespan < eval.makespan) ||
                    (candidate_eval.makespan == eval.makespan && candidate_idle < current_idle);
                if (!improves)
                    continue;

                if (!gap_fill_found ||
                    candidate_eval.makespan < best_gap_eval.makespan ||
                    (candidate_eval.makespan == best_gap_eval.makespan &&
                     candidate_idle < best_gap_idle))
                {
                    gap_fill_found = true;
                    best_gap_state = std::move(candidate);
                    best_gap_eval = std::move(candidate_eval);
                    best_gap_idle = candidate_idle;
                }
            }
        }

        if (gap_fill_found)
        {
            state = std::move(best_gap_state);
            return true;
        }

        std::uniform_int_distribution<size_t> pick(0, shortlist - 1);
        const auto &choice = candidates[pick(rng)];
        const auto blocker = choice.blocker;
        if (blocker >= state.proc_of.size())
            return false;

        const auto old_proc = state.proc_of[blocker];
        const auto current_tier = state.tier_of[blocker];
        const auto earliest_old_tier = earliestLegalTierOnProcessor(state, graph, blocker, old_proc);

        if (current_tier > earliest_old_tier)
        {
            std::vector<size_t> targets;
            targets.push_back(earliest_old_tier);
            if (current_tier > earliest_old_tier + 1)
                targets.push_back((earliest_old_tier + current_tier) / 2);
            if (current_tier > 0)
                targets.push_back(current_tier - 1);

            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
            for (const auto target : targets)
            {
                if (target < current_tier &&
                    scheduling_problem::algorithms::moveTaskToTier(state, blocker, target))
                {
                    return true;
                }
            }
        }

        std::vector<unsigned> proc_order;
        proc_order.reserve(state.processors.size());
        if (choice.processor != old_proc)
            proc_order.push_back(choice.processor);
        for (unsigned p = 0; p < state.processors.size(); ++p)
        {
            if (p != old_proc && p != choice.processor)
                proc_order.push_back(p);
        }
        std::stable_sort(proc_order.begin(), proc_order.end(),
                         [&](unsigned a, unsigned b)
                         {
                             if (state.processors[a].size() != state.processors[b].size())
                                 return state.processors[a].size() < state.processors[b].size();
                             return a < b;
                         });

        for (const auto new_proc : proc_order)
        {
            const auto earliest = earliestLegalTierOnProcessor(state, graph, blocker, new_proc);
            const auto latest = latestLegalInsertionTier(state, graph, blocker, new_proc);
            if (earliest > latest)
                continue;

            std::vector<size_t> targets{earliest};
            if (earliest < latest)
                targets.push_back(std::min(latest, earliest + 1));

            std::sort(targets.begin(), targets.end());
            targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
            for (const auto target : targets)
            {
                if (target <= latest &&
                    scheduling_problem::algorithms::moveTaskToProcessor(state, blocker, new_proc, target))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool proposeBestNeighbor(const scheduling_problem::Graph &graph,
                             const scheduling_problem::algorithms::LayeredState &current,
                             const scheduling_problem::algorithms::LayeredEval &current_eval,
                             const std::vector<size_t> &priority_order,
                             scheduling_problem::weight_t memory_limit,
                             double overflow_penalty,
                             double fedorenko_idle_prob,
                             std::mt19937 &rng,
                             unsigned neighbor_trials,
                             unsigned perturbation_depth,
                             scheduling_problem::algorithms::LayeredState &best_state,
                             scheduling_problem::algorithms::LayeredEval &best_eval,
                             double &best_score,
                             FedorenkoProposalStats *fedorenko_stats = nullptr)
    {
        bool found = false;
        const auto trials = std::max(1u, neighbor_trials);
        const auto depth = std::max(1u, perturbation_depth);
        const auto dedicated_fedorenko_trials =
            (fedorenko_idle_prob > 0.0)
                ? std::min(trials,
                           std::max(1u,
                                    static_cast<unsigned>(std::ceil(fedorenko_idle_prob * trials * 1.5))))
                : 0u;

        for (unsigned t = 0; t < trials; ++t)
        {
            auto candidate = current;
            bool moved = false;
            bool candidate_used_fedorenko = false;
            const bool force_fedorenko = t < dedicated_fedorenko_trials;
            for (unsigned d = 0; d < depth; ++d)
            {
                bool step_moved = false;
                if (d == 0 && fedorenko_idle_prob > 0.0)
                {
                    bool attempted_fedorenko = false;
                    if (force_fedorenko)
                    {
                        attempted_fedorenko = true;
                        step_moved = applyFedorenkoIdleReduction(candidate, current_eval, graph, memory_limit, rng);
                    }
                    else
                    {
                        std::bernoulli_distribution use_idle_repair(std::min(1.0, fedorenko_idle_prob));
                        if (use_idle_repair(rng))
                        {
                            attempted_fedorenko = true;
                            step_moved = applyFedorenkoIdleReduction(candidate, current_eval, graph, memory_limit, rng);
                        }
                    }
                    if (attempted_fedorenko && fedorenko_stats)
                        fedorenko_stats->attempted_trials++;
                    if (attempted_fedorenko && step_moved)
                    {
                        candidate_used_fedorenko = true;
                        if (fedorenko_stats)
                            fedorenko_stats->successful_trials++;
                    }
                }
                if (!step_moved)
                    step_moved = applyRandomLayeredMove(candidate, priority_order, rng);
                moved = step_moved || moved;
            }
            if (!moved)
                continue;

            auto eval = scheduling_problem::algorithms::evaluateLayeredState(graph, candidate, memory_limit);
            if (!eval.feasible())
                continue;
            const auto score = eval.score(overflow_penalty);
            if (!found || score < best_score)
            {
                found = true;
                best_state = std::move(candidate);
                best_eval = std::move(eval);
                best_score = score;
                if (fedorenko_stats)
                    fedorenko_stats->best_candidate_used_fedorenko = candidate_used_fedorenko;
            }
        }
        return found;
    }

    bool applyFedorenkoRepair(scheduling_problem::algorithms::LayeredState &state,
                              scheduling_problem::algorithms::LayeredEval &eval,
                              double &score,
                              const scheduling_problem::Graph &graph,
                              scheduling_problem::weight_t memory_limit,
                              double overflow_penalty,
                              double fedorenko_idle_prob,
                              std::mt19937 &rng,
                              FedorenkoRepairStats *repair_stats = nullptr)
    {
        if (fedorenko_idle_prob <= 0.0)
            return false;

        const auto repair_passes =
            std::max(1u, static_cast<unsigned>(std::lround(1.0 + 3.0 * fedorenko_idle_prob)));
        bool improved_any = false;

        for (unsigned pass = 0; pass < repair_passes; ++pass)
        {
            auto candidate = state;
            if (repair_stats)
                repair_stats->attempts++;
            if (!applyFedorenkoIdleReduction(candidate, eval, graph, memory_limit, rng))
                break;

            auto candidate_eval = scheduling_problem::algorithms::evaluateLayeredState(graph, candidate, memory_limit);
            if (!candidate_eval.feasible())
                continue;

            const auto candidate_score = candidate_eval.score(overflow_penalty);
            const auto current_idle = computeInternalIdle(state, eval);
            const auto candidate_idle = computeInternalIdle(candidate, candidate_eval);

            const bool improves =
                (candidate_score < score) ||
                (candidate_score == score && candidate_idle < current_idle);
            if (!improves)
                continue;

            state = std::move(candidate);
            eval = std::move(candidate_eval);
            score = candidate_score;
            improved_any = true;
            if (repair_stats)
                repair_stats->improvements++;
        }

        return improved_any;
    }
}

namespace scheduling_problem::algorithms
{
    SimulatedAnnealing::SimulatedAnnealing(const BaseOptimization &baseline,
                                           double min_temp,
                                           double max_temp,
                                           ReductionRules reduction_rule,
                                           unsigned saturation,
                                           double improvement,
                                           unsigned seed,
                                           const std::string &label)
        : IterativeOptimization(baseline, saturation, improvement, seed, label),
          min_temp_(min_temp),
          max_temp_(max_temp),
          reduction_rule_(reduction_rule)
    {
        setReductionRule(reduction_rule);
    }

    bool SimulatedAnnealing::isTransitionAcceptance(double energy_delta, double temperature)
    {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        const auto prob = dist(rng_);
        const auto transition_prob = energy_delta > 0.0
                                         ? std::exp(-energy_delta / temperature)
                                         : 1.1;
        probs_dynamic.push_back(transition_prob);
        return prob <= transition_prob;
    }

    void SimulatedAnnealing::setReductionRule(ReductionRules reduction_rule)
    {
        reduction_rule_ = reduction_rule;

        switch (reduction_rule_)
        {
        case ReductionRules::boltzmann:
            reduceTemperature_ = boltzmannRule;
            break;
        case ReductionRules::couchy:
            reduceTemperature_ = couchyRule;
            break;
        case ReductionRules::mixed:
            reduceTemperature_ = mixedRule;
            break;
        default:
            reduceTemperature_ = boltzmannRule;
            break;
        }
    }

    void SimulatedAnnealing::setParams(const ParamSet &params)
    {
        IterativeOptimization::setParams(params);
        for (const auto &[name, val] : params)
        {
            if (name == "min_temp")
            {
                min_temp_ = (double)val;
            }
            else if (name == "max_temp")
            {
                max_temp_ = (double)val;
            }
            else if (name == "reduction_rule")
            {
                setReductionRule((ReductionRules)(unsigned)val);
            }
            else if (name == "processors")
            {
                processors_ = std::max(1u, (unsigned)val);
                if (baseline_)
                    baseline_->setParams({{"processors", processors_}});
            }
            else if (name == "memory_limit")
            {
                const auto raw = (double)val;
                const auto inf_like = static_cast<double>(std::numeric_limits<weight_t>::max() / 2);
                if (raw >= inf_like)
                    memory_limit_ = std::numeric_limits<weight_t>::max();
                else
                    memory_limit_ = static_cast<weight_t>(raw);
                if (baseline_)
                    baseline_->setParams({{"memory_limit", static_cast<double>(memory_limit_)}});
            }
            else if (name == "overflow_penalty")
            {
                overflow_penalty_ = std::max(1.0, (double)val);
            }
            else if (name == "neighbor_trials")
            {
                neighbor_trials_ = std::max(1u, (unsigned)val);
            }
            else if (name == "perturbation_depth")
            {
                perturbation_depth_ = std::max(1u, (unsigned)val);
            }
            else if (name == "restart_period")
            {
                restart_period_ = (unsigned)val;
            }
            else if (name == "max_iters")
            {
                max_iters_ = (unsigned)val;
            }
            else if (name == "restarts")
            {
                restarts_ = std::max(1u, (unsigned)val);
            }
            else if (name == "kick_moves")
            {
                kick_moves_ = (unsigned)val;
            }
            else if (name == "fedorenko_idle_prob")
            {
                fedorenko_idle_prob_ = std::clamp((double)val, 0.0, 1.0);
            }
        }
    }

    ParamSet SimulatedAnnealing::getParams() const
    {
        auto params = IterativeOptimization::getParams();
        params["min_temp"] = min_temp_;
        params["max_temp"] = max_temp_;
        params["reduction_rule"] = (unsigned)reduction_rule_;
        params["processors"] = processors_;
        params["memory_limit"] = static_cast<double>(memory_limit_);
        params["overflow_penalty"] = overflow_penalty_;
        params["neighbor_trials"] = neighbor_trials_;
        params["perturbation_depth"] = perturbation_depth_;
        params["restart_period"] = restart_period_;
        params["max_iters"] = max_iters_;
        params["restarts"] = restarts_;
        params["kick_moves"] = kick_moves_;
        params["fedorenko_idle_prob"] = fedorenko_idle_prob_;
        return params;
    }

    const SimulatedAnnealing::FedorenkoDiagnostics &SimulatedAnnealing::fedorenkoDiagnostics() const
    {
        return fedorenko_diag_;
    }

    Schedule SimulatedAnnealing::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        probs_dynamic.clear();
        temps_dynamic.clear();
        improvement_dynamic.clear();
        lambda_dynamic.clear();
        fedorenko_diag_ = {};

        rng_.seed(mixSeed(seed_, static_cast<unsigned>(boost::num_vertices(graph))));

        const auto bottom_levels = computeBottomLevels(graph);
        const auto priority_order = makePriorityOrder(bottom_levels);

        LayeredState initial = makeStateFromSchedule(graph, base_schedule, processors_);
        LayeredEval initial_eval = evaluateLayeredState(graph, initial, memory_limit_);

        auto priority_state = makePriorityState(graph, processors_, bottom_levels);
        auto priority_eval = evaluateLayeredState(graph, priority_state, memory_limit_);
        if (betterEval(priority_eval, initial_eval, overflow_penalty_))
        {
            initial = std::move(priority_state);
            initial_eval = std::move(priority_eval);
        }

        auto list_state = makeListState(graph, processors_);
        auto list_eval = evaluateLayeredState(graph, list_state, memory_limit_);
        if (betterEval(list_eval, initial_eval, overflow_penalty_))
        {
            initial = std::move(list_state);
            initial_eval = std::move(list_eval);
        }

        double initial_score = initial_eval.score(overflow_penalty_);
        cost_dynamics_.push_back(clampObjectiveTrace(initial_score));

        LayeredState best = initial;
        LayeredEval best_eval = initial_eval;
        double best_score = initial_score;
        bool have_feasible = initial_eval.feasible();

        iters_count_ = 0;
        const auto restart_count = std::max(1u, restarts_);
        const unsigned per_restart_budget =
            (max_iters_ == 0) ? 0 : std::max(1u, max_iters_ / restart_count);
        const unsigned max_empty_rounds =
            std::max(32u, std::max(1u, neighbor_trials_) * std::max(2u, perturbation_depth_) * 8u);

        for (unsigned restart = 0; restart < restart_count; ++restart)
        {
            LayeredState current = (restart == 0) ? initial : best;
            LayeredEval current_eval = (restart == 0) ? initial_eval : best_eval;
            double current_score = (restart == 0) ? initial_score : best_score;

            if (restart > 0 && kick_moves_ > 0)
            {
                const auto kicks = std::max(1u, kick_moves_);
                for (unsigned k = 0; k < kicks; ++k)
                    applyRandomLayeredMove(current, priority_order, rng_);

                current_eval = evaluateLayeredState(graph, current, memory_limit_);
                if (!current_eval.feasible())
                {
                    current = best;
                    current_eval = best_eval;
                    current_score = best_score;
                }
                else
                {
                    current_score = current_eval.score(overflow_penalty_);
                }
            }

            unsigned stagnations_count = 0;
            unsigned local_iter = 0;
            unsigned empty_rounds = 0;
            while (true)
            {
                const auto thermal_iter = local_iter + 1;
                const auto current_temp = reduceTemperature_(max_temp_, thermal_iter);
                if (current_temp <= min_temp_)
                    break;
                if (per_restart_budget && local_iter >= per_restart_budget)
                    break;
                if (max_iters_ && iters_count_ >= max_iters_)
                    break;

                LayeredState candidate;
                LayeredEval candidate_eval;
                double candidate_score = std::numeric_limits<double>::max();
                FedorenkoProposalStats proposal_stats{};
                if (proposeBestNeighbor(graph,
                                        current,
                                        current_eval,
                                        priority_order,
                                        memory_limit_,
                                        overflow_penalty_,
                                        fedorenko_idle_prob_,
                                        rng_,
                                        neighbor_trials_,
                                        perturbation_depth_,
                                        candidate,
                                        candidate_eval,
                                        candidate_score,
                                        &proposal_stats))
                {
                    fedorenko_diag_.neighbor_trials_with_attempt += proposal_stats.attempted_trials;
                    fedorenko_diag_.neighbor_trials_with_success += proposal_stats.successful_trials;
                    if (proposal_stats.best_candidate_used_fedorenko)
                        fedorenko_diag_.best_candidate_hits++;
                    empty_rounds = 0;
                    const auto energy_delta = candidate_score - current_score;
                    if (isTransitionAcceptance(energy_delta, current_temp))
                    {
                        fedorenko_diag_.accepted_transitions_total++;
                        if (proposal_stats.best_candidate_used_fedorenko)
                            fedorenko_diag_.accepted_transition_hits++;
                        current = std::move(candidate);
                        current_eval = std::move(candidate_eval);
                        current_score = candidate_score;
                        FedorenkoRepairStats repair_stats{};
                        applyFedorenkoRepair(current,
                                             current_eval,
                                             current_score,
                                             graph,
                                             memory_limit_,
                                             overflow_penalty_,
                                             fedorenko_idle_prob_,
                                             rng_,
                                             &repair_stats);
                        fedorenko_diag_.repair_attempts += repair_stats.attempts;
                        fedorenko_diag_.repair_improvements += repair_stats.improvements;
                    }
                }
                else
                {
                    empty_rounds++;
                    if (restart_period_ && empty_rounds >= max_empty_rounds)
                    {
                        current = best;
                        current_eval = best_eval;
                        current_score = best_score;
                        empty_rounds = 0;
                    }
                    else if (empty_rounds >= max_empty_rounds * 2)
                    {
                        break;
                    }
                    continue;
                }

                bool improved = false;
                const auto prev_best = best_score;

                if (current_eval.feasible())
                {
                    if (!have_feasible || current_score < best_score)
                    {
                        best = current;
                        best_eval = current_eval;
                        best_score = current_score;
                        have_feasible = true;
                        improved = true;
                    }
                }
                else if (!have_feasible && current_score < best_score)
                {
                    best = current;
                    best_eval = current_eval;
                    best_score = current_score;
                    improved = true;
                }

                iters_count_++;
                local_iter++;

                if (improved)
                {
                    const auto denom = std::max(1.0, std::abs(prev_best));
                    const auto rel_improvement = (prev_best - best_score) / denom;
                    if (!saturation_ || rel_improvement > improvement_)
                        stagnations_count = 0;
                    else
                        stagnations_count++;
                }
                else
                {
                    stagnations_count++;
                }

                if (restart_period_ && stagnations_count > 0 && (stagnations_count % restart_period_ == 0))
                {
                    current = best;
                    current_eval = best_eval;
                    current_score = best_score;
                }

                cost_dynamics_.push_back(clampObjectiveTrace(current_score));
                temps_dynamic.push_back(current_temp);
                improvement_dynamic.push_back(stagnations_count);

                if (saturation_ && stagnations_count >= saturation_)
                    break;
            }

            if (max_iters_ && iters_count_ >= max_iters_)
                break;
        }

        if (!have_feasible && !isUnlimited(memory_limit_))
        {
            throw std::runtime_error(
                "SAO could not find a feasible layered schedule under memory limit " +
                std::to_string(memory_limit_) + " for graph '" + graph.name() + "'.");
        }

        auto out = toSchedule(graph, best, best_eval);
        if (have_feasible)
            out.setCost(best_eval.makespan);
        else
            out.setCost(clampObjectiveTrace(best_score));
        return out;
    }

    double SimulatedAnnealing::boltzmannRule(double init_temp, size_t iter_num)
    {
        return init_temp / std::log2(1.0 + (double)iter_num);
    }

    double SimulatedAnnealing::couchyRule(double init_temp, size_t iter_num)
    {
        return init_temp / (1.0 + (double)iter_num);
    }

    double SimulatedAnnealing::mixedRule(double init_temp, size_t iter_num)
    {
        return init_temp * std::log2(1.0 + (double)iter_num) / (1.0 + (double)iter_num);
    }

    std::unique_ptr<BaseOptimization> SimulatedAnnealing::copy() const
    {
        auto ptr = std::unique_ptr<SimulatedAnnealing>(
            new SimulatedAnnealing(*baseline_, min_temp_, max_temp_, reduction_rule_,
                                   saturation_, improvement_, seed_, label_));
        ptr->setParams({
            {"processors", processors_},
            {"memory_limit", static_cast<double>(memory_limit_)},
            {"overflow_penalty", overflow_penalty_},
            {"neighbor_trials", neighbor_trials_},
            {"perturbation_depth", perturbation_depth_},
            {"restart_period", restart_period_},
            {"max_iters", max_iters_},
            {"restarts", restarts_},
            {"kick_moves", kick_moves_},
            {"fedorenko_idle_prob", fedorenko_idle_prob_}
        });
        return ptr;
    }
}
