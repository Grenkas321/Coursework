#include "SimulatedAnnealing.h"

namespace scheduling_problem::algorithms
{
    /**
     * Construct a simulated annealing optimizer.
     *
     * @param baseline        Baseline optimization used to produce/interpret schedules.
     * @param min_temp        Lower bound for the temperature; loop stops when T <= min_temp.
     * @param max_temp        Initial temperature for the annealing process.
     * @param reduction_rule  Temperature reduction rule (Boltzmann, Cauchy, or mixed).
     * @param saturation      Optional early-stop limit on stagnating iterations (0 disables).
     * @param improvement     Relative improvement threshold that resets stagnation counter.
     * @param seed            RNG seed for reproducibility.
     * @param label           Human-readable label for this optimizer instance.
     */
    SimulatedAnnealing::SimulatedAnnealing(const BaseOptimization &baseline,
                                           double min_temp,
                                           double max_temp,
                                           ReductionRules reduction_rule,
                                           unsigned saturation,
                                           double improvement,
                                           unsigned seed,
                                           const std::string &label)
        : IterativeOptimization(baseline, saturation,
                                improvement, seed, label),
          min_temp_(min_temp), max_temp_(max_temp), reduction_rule_(reduction_rule), rebuilder_(seed)
    {
        setReductionRule(reduction_rule);
    }

    /**
     * Decide whether to accept a transition with the given energy delta at a temperature.
     *
     * @param energy_delta  Difference in objective (next_cost - curr_cost); negative is better.
     * @param temperature   Current annealing temperature.
     * @return true if the transition is accepted (always for non-worsening moves),
     *         otherwise with probability exp(-delta / T).
     */
    bool SimulatedAnnealing::isTransitionAcceptance(double energy_delta, double temperature)
    {
        std::uniform_real_distribution<double> dist(0, 1);
        double prob = dist(rng_);
        double transition_prob = energy_delta > 0 ? exp(-(double)energy_delta / temperature) : 1.1;
        probs_dynamic.push_back(transition_prob);
        return prob <= transition_prob;
    }

    /**
     * Run the simulated annealing loop starting from a baseline schedule.
     *
     * The method cools the temperature according to the selected reduction rule.
     * On each iteration a random local transform proposes a neighbor; it is accepted
     * based on the Metropolis criterion. The best-so-far schedule is tracked and
     * returned. Optional stagnation control may interrupt the loop early.
     *
     * @param graph          Input DAG to schedule.
     * @param base_schedule  Starting schedule (typically from the baseline optimizer).
     * @return Best schedule found during annealing.
     */
    Schedule SimulatedAnnealing::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        auto best_solution = base_schedule;
        ScheduleStatus status(graph, best_solution);
        unsigned stagnations_count = 0;
        double current_temp;
        iters_count_ = 0;

        while ((current_temp = reduceTemperature_(max_temp_, iters_count_ + 1)) > min_temp_)
        {
            auto curr_cost = status.cost();
            auto next_cost = rebuilder_.transform(graph, status);
            auto energy_delta = (double)next_cost - curr_cost;
            if (!isTransitionAcceptance(energy_delta, current_temp))
                rebuilder_.invtransform(graph, status);
            if (status.cost() < best_solution.cost())
            {
                auto improvement_percentage = ((double)best_solution.cost() - status.cost()) / best_solution.cost();
                if (saturation_ && improvement_percentage > improvement_)
                    stagnations_count = 0;
                best_solution = status;
            }
            iters_count_++;
            stagnations_count++;
            if (saturation_ && stagnations_count > saturation_)
            {
                break;
            }

            cost_dynamics_.push_back(status.cost());
            temp_dynamic.push_back(current_temp);
        }
        return best_solution;
    }

    /**
     * Polymorphic clone.
     *
     * @return New instance with identical parameters and baseline reference.
     */
    std::unique_ptr<BaseOptimization> SimulatedAnnealing::copy() const
    {
        return std::unique_ptr<SimulatedAnnealing>(new SimulatedAnnealing(*baseline_, min_temp_, max_temp_, reduction_rule_,
                                                                          saturation_, improvement_, seed_, label_));
    }

    /**
     * Boltzmann temperature schedule.
     *
     * @param init_temp  Initial temperature (T0).
     * @param iter_num   Iteration number (1-based).
     * @return Temperature T(iter) = T0 / log2(1 + iter).
     */
    double SimulatedAnnealing::boltzmannRule(double init_temp, size_t iter_num)
    {
        return init_temp / std::log2(1 + iter_num);
    }

    /**
     * Cauchy temperature schedule.
     *
     * @param init_temp  Initial temperature (T0).
     * @param iter_num   Iteration number (1-based).
     * @return Temperature T(iter) = T0 / (1 + iter).
     */
    double SimulatedAnnealing::couchyRule(double init_temp, size_t iter_num)
    {
        return init_temp / (1 + iter_num);
    }

    /**
     * Mixed temperature schedule (Boltzmann-like early, Cauchy-like late).
     *
     * @param init_temp  Initial temperature (T0).
     * @param iter_num   Iteration number (1-based).
     * @return Temperature T(iter) = T0 * log2(1 + iter) / (1 + iter).
     */
    double SimulatedAnnealing::mixedRule(double init_temp, size_t iter_num)
    {
        return init_temp * std::log2(1 + iter_num) / (1 + iter_num);
    }

    /**
     * Select the active temperature reduction rule.
     *
     * @param reduction_rule  Rule to apply while cooling.
     * @throws ...            If an unknown rule is supplied.
     */
    void SimulatedAnnealing::setReductionRule(ReductionRules reduction_rule)
    {
        switch (reduction_rule)
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
            throw;
        }
    }

    /**
     * Set optimizer parameters by name.
     *
     * Recognized keys:
     *  - "min_temp" (double)
     *  - "max_temp" (double)
     *  - "reduction_rule" (unsigned cast to ReductionRules)
     * Others are forwarded to IterativeOptimization::setParams.
     *
     * @param params  Map of parameter names to values.
     */
    void SimulatedAnnealing::setParams(const ParamSet &params)
    {
        for (const auto &param : params)
        {
            if (param.first == "min_temp")
                min_temp_ = param.second;
            else if (param.first == "max_temp")
                max_temp_ = param.second;
            else if (param.first == "reduction_rule")
            {
                reduction_rule_ = (ReductionRules)(unsigned)param.second;
                setReductionRule(reduction_rule_);
            }
            else
                IterativeOptimization::setParams({param});
        }
    }

    /**
     * Get the current parameter set including inherited ones.
     *
     * @return Aggregated parameter map with keys:
     *         "min_temp", "max_temp", "reduction_rule", plus iterative base params.
     */
    ParamSet SimulatedAnnealing::getParams() const
    {
        auto params = IterativeOptimization::getParams();
        params["min_temp"] = min_temp_;
        params["max_temp"] = max_temp_;
        params["reduction_rule"] = (unsigned)reduction_rule_;
        return params;
    }
}
