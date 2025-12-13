#pragma once

#include "ScheduleCorrector.h"
#include "IterativeOptimization.h"

namespace scheduling_problem::algorithms
{
    /**
     * @brief Simulated annealing optimizer.
     *
     * Iteratively applies random schedule transformations and accepts them
     * under a temperature-driven Metropolis criterion.
     */
    class SimulatedAnnealing : public IterativeOptimization
    {
    public:
        /**
         * @brief Temperature reduction laws.
         */
        enum class ReductionRules
        {
            /** Boltzmann cooling: T_k = T0 / log2(1 + k). */
            boltzmann,
            /** Cauchy cooling:   T_k = T0 / (1 + k). */
            couchy,
            /** Mixed cooling:    T_k = T0 * log2(1 + k) / (1 + k). */
            mixed
        };

        /**
         * @brief Construct a simulated annealing optimizer.
         *
         * @param baseline        Baseline optimization used to produce/interpret schedules.
         * @param min_temp        Lower bound for the temperature; loop stops when T <= min_temp.
         * @param max_temp        Initial temperature for the annealing process.
         * @param reduction_rule  Temperature reduction rule.
         * @param saturation      Optional early-stop limit on stagnating iterations (0 disables).
         * @param improvement     Improvement threshold for resetting stagnations.
         * @param seed            RNG seed (может быть игнорирован, если seed переопределяется в schedule_).
         * @param label           Algorithm label for output.
         */
        SimulatedAnnealing(const BaseOptimization &baseline = BASELINE,
                           double min_temp = 0.1,
                           double max_temp = 10.0,
                           ReductionRules reduction_rule = ReductionRules::boltzmann,
                           unsigned saturation = 0,
                           double improvement = 0.0,
                           unsigned seed = 42,
                           const std::string &label = "sao");

        /**
         * @brief Set the cooling rule.
         */
        void setReductionRule(ReductionRules reduction_rule);

        std::unique_ptr<BaseOptimization> copy() const override;


        /**
         * Decide whether to accept a transition with the given energy delta at a temperature.
         */
        bool isTransitionAcceptance(double energy_delta, double temperature);

        std::vector<double> probs_dynamic;
        std::vector<double> cost_dynamic;
        std::vector<double> temps_dynamic;
        std::vector<double> improvement_dynamic;
        std::vector<double> lambda_dynamic;

    protected:
        /** @brief Lower/upper temperature bounds. */
        double min_temp_, max_temp_;
        /** @brief Active cooling rule. */
        ReductionRules reduction_rule_;
        /** @brief Function implementing the selected cooling schedule. */
        std::function<double(double, size_t)> reduceTemperature_;
        /** @brief Local schedule transformer used to explore neighbors. */
        ScheduleCorrector rebuilder_;

        /**
         * @brief Main SAO loop (override from IterativeOptimization).
         */
        virtual Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;

        static double boltzmannRule(double init_temp, size_t iter_num);
        static double couchyRule(double init_temp, size_t iter_num);
        static double mixedRule(double init_temp, size_t iter_num);
    };
}
