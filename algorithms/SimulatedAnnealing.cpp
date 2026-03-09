#include "SimulatedAnnealing.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

#include "LayeredSchedule.h"

namespace
{
    static unsigned runtime_seed(unsigned salt = 0u)
    {
        std::random_device rd;
        const unsigned t =
            (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return rd() ^ (t + 0x9e3779b9u + (salt << 6) + (salt >> 2));
    }

    inline bool isUnlimited(scheduling_problem::weight_t limit)
    {
        return limit == std::numeric_limits<scheduling_problem::weight_t>::max();
    }

    bool applyRandomLayeredMove(scheduling_problem::algorithms::LayeredState &state,
                                std::mt19937 &rng)
    {
        const size_t n = state.proc_of.size();
        if (n == 0 || state.processors.empty())
            return false;

        std::uniform_int_distribution<size_t> task_dist(0, n - 1);
        std::bernoulli_distribution op_kind(0.5); // true: O1(move proc), false: O2(move tier)

        for (unsigned attempt = 0; attempt < 64; ++attempt)
        {
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
            else if (name == "overflow_penalty")
            {
                overflow_penalty_ = std::max(1.0, (double)val);
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
        return params;
    }

    Schedule SimulatedAnnealing::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        probs_dynamic.clear();
        temps_dynamic.clear();
        improvement_dynamic.clear();
        lambda_dynamic.clear();

        const unsigned run_seed = runtime_seed(seed_);
        rng_.seed(run_seed);

        LayeredState current = makeStateFromSchedule(graph, base_schedule, processors_);
        LayeredEval current_eval = evaluateLayeredState(graph, current, memory_limit_);
        double current_score = current_eval.score(overflow_penalty_);

        LayeredState best = current;
        LayeredEval best_eval = current_eval;
        double best_score = current_score;
        bool have_feasible = current_eval.feasible();

        unsigned stagnations_count = 0;
        iters_count_ = 0;

        double current_temp = max_temp_;
        while ((current_temp = reduceTemperature_(max_temp_, iters_count_ + 1)) > min_temp_)
        {
            auto candidate = current;
            if (applyRandomLayeredMove(candidate, rng_))
            {
                auto candidate_eval = evaluateLayeredState(graph, candidate, memory_limit_);
                const auto candidate_score = candidate_eval.score(overflow_penalty_);
                const auto energy_delta = candidate_score - current_score;

                if (isTransitionAcceptance(energy_delta, current_temp))
                {
                    current = std::move(candidate);
                    current_eval = std::move(candidate_eval);
                    current_score = candidate_score;
                }
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

            cost_dynamics_.push_back((weight_t)std::llround(best_score));
            temps_dynamic.push_back(current_temp);
            improvement_dynamic.push_back(stagnations_count);

            if (saturation_ && stagnations_count >= saturation_)
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
            out.setCost((weight_t)std::llround(best_score));
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
            {"overflow_penalty", overflow_penalty_}
        });
        return ptr;
    }
}
