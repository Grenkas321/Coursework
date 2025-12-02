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
         * @brief Temperature trace per iteration.
         */
        std::vector<double> temp_dynamic;
        /**
         * @brief Acceptance-probability trace and auxiliary lambda trace.
         */
        std::vector<double> probs_dynamic, lambda_dynamic;

    protected:
        /** @brief Lower/upper temperature bounds. */
        double min_temp_, max_temp_;
        /** @brief Active cooling rule. */
        ReductionRules reduction_rule_;
        /** @brief Function implementing the selected cooling schedule. */
        std::function<double(double, size_t)> reduceTemperature_;
        /** @brief Local schedule transformer used to explore neighbors. */
        ScheduleCorrector rebuilder_;

    public:
        /**
         * @brief Construct a simulated annealing optimizer.
         *
         * @param baseline        Baseline optimizer used to obtain/interpret schedules.
         * @param min_temp        Minimal temperature; cooling stops when T <= min_temp.
         * @param max_temp        Initial temperature at the first iteration.
         * @param reduction_rule  Cooling rule to use (see ReductionRules).
         * @param saturation      Max consecutive non-significant improvements (0 disables).
         * @param improvement     Relative improvement threshold to reset stagnation counter.
         * @param seed            Random seed for reproducibility.
         * @param label           Human-readable label for this optimizer.
         */
        SimulatedAnnealing(const BaseOptimization &baseline = BASELINE,
                           double min_temp = 1,
                           double max_temp = 13,
                           ReductionRules reduction_rule = ReductionRules::boltzmann,
                           unsigned saturation = 0,
                           double improvement = 0,
                           unsigned seed = 42,
                           const std::string &label = "sao");

        /**
         * @brief Copy constructor.
         */
        SimulatedAnnealing(const SimulatedAnnealing &other) = default;

        /**
         * @brief Polymorphic clone.
         * @return A new heap-allocated copy of this optimizer.
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;

        /**
         * @brief Set parameters by name.
         *
         * Recognized keys:
         *  - "min_temp" (double)
         *  - "max_temp" (double)
         *  - "reduction_rule" (unsigned cast to ReductionRules)
         * All other keys are forwarded to IterativeOptimization::setParams.
         *
         * @param params Parameter map.
         */
        virtual void setParams(const ParamSet &params);

        /**
         * @brief Get the current parameter map (including inherited ones).
         * @return Map containing "min_temp", "max_temp", "reduction_rule" and base params.
         */
        virtual ParamSet getParams() const;

        /**
         * @brief Destructor.
         */
        virtual ~SimulatedAnnealing() = default;

    protected:
        /**
         * @brief Run the annealing loop starting from a base schedule.
         *
         * Applies random local transforms, accepts them with a temperature-dependent
         * probability, tracks the best-so-far schedule, and optionally stops early
         * on stagnation.
         *
         * @param graph          Input DAG.
         * @param base_schedule  Initial schedule (typically from the baseline).
         * @return Best schedule found.
         */
        virtual Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;

        /**
         * @brief Metropolis acceptance test.
         *
         * @param energy_delta  Difference in objective (next_cost - curr_cost).
         *                      Negative values indicate an improvement.
         * @param temperature   Current temperature.
         * @return true if the move is accepted; always true for improvements,
         *         otherwise with probability exp(-delta / T).
         */
        bool isTransitionAcceptance(double energy_delta, double temperature);

        /**
         * @brief Select the cooling rule to use.
         * @param reduction_rule New cooling rule.
         */
        void setReductionRule(ReductionRules reduction_rule);

        /**
         * @brief Boltzmann cooling schedule.
         * @param init_temp  Initial temperature (T0).
         * @param iter_num   Iteration index (1-based).
         * @return Temperature T(iter).
         */
        static double boltzmannRule(double init_temp, size_t iter_num);

        /**
         * @brief Cauchy cooling schedule.
         * @param init_temp  Initial temperature (T0).
         * @param iter_num   Iteration index (1-based).
         * @return Temperature T(iter).
         */
        static double couchyRule(double init_temp, size_t iter_num);

        /**
         * @brief Mixed cooling schedule.
         * @param init_temp  Initial temperature (T0).
         * @param iter_num   Iteration index (1-based).
         * @return Temperature T(iter).
         */
        static double mixedRule(double init_temp, size_t iter_num);
    };
}
