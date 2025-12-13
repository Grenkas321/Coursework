#include "SimulatedAnnealing.h"
#include <random>
#include <chrono>
#include <cstddef>

namespace {
    // Всегда получаем новый seed для каждого запуска алгоритма.
    static unsigned runtime_seed(unsigned salt = 0u)
    {
        std::random_device rd;
        const unsigned t = (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        // mix
        return rd() ^ (t + 0x9e3779b9u + (salt << 6) + (salt >> 2));
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
        : IterativeOptimization(baseline, saturation,
                                improvement, seed, label),
          min_temp_(min_temp), max_temp_(max_temp), reduction_rule_(reduction_rule), rebuilder_(seed)
    {
        setReductionRule(reduction_rule);
    }

    bool SimulatedAnnealing::isTransitionAcceptance(double energy_delta, double temperature)
    {
        std::uniform_real_distribution<double> dist(0, 1);
        double prob = dist(rng_);
        double transition_prob = energy_delta > 0 ? exp(-(double)energy_delta / temperature) : 1.1;
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

    Schedule SimulatedAnnealing::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        // RNG: каждый запуск SAO должен быть стохастическим (новый seed).
        const unsigned run_seed = runtime_seed(seed_);
        rng_.seed(run_seed);
        ScheduleCorrector local_rebuilder(run_seed);

        auto best_solution = base_schedule;
        ScheduleStatus status(graph, best_solution);
        unsigned stagnations_count = 0;
        double current_temp;
        iters_count_ = 0;

        while ((current_temp = reduceTemperature_(max_temp_, iters_count_ + 1)) > min_temp_)
        {
            auto curr_cost = status.cost();
            auto next_cost = local_rebuilder.transform(graph, status);
            auto energy_delta = (double)next_cost - curr_cost;
            if (!isTransitionAcceptance(energy_delta, current_temp))
                local_rebuilder.invtransform(graph, status);

            if (status.cost() < best_solution.cost())
            {
                auto improvement_percentage = ((double)best_solution.cost() - status.cost()) / best_solution.cost();
                if (saturation_ && improvement_percentage > improvement_)
                    stagnations_count = 0;
                best_solution = status;
            }

            iters_count_++;
            stagnations_count++;
            cost_dynamics_.push_back(best_solution.cost());
            temps_dynamic.push_back(current_temp);
            improvement_dynamic.push_back(stagnations_count);

            if (saturation_ && stagnations_count >= saturation_)
                break;
        }
        return best_solution;
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
    return std::unique_ptr<SimulatedAnnealing>(
        new SimulatedAnnealing(*baseline_, min_temp_, max_temp_, reduction_rule_,
                               saturation_, improvement_, seed_, label_));
}

}
