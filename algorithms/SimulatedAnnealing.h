#pragma once

#include "ScheduleCorrector.h"
#include "IterativeOptimization.h"

namespace scheduling_problem::algorithms
{
    /**
     * Simulated annealing algorithm implementaton
     */
    class SimulatedAnnealing : public IterativeOptimization
    {

    public:
        /**
         * Possible temperature reduction laws
         */
        enum class ReductionRules
        {
            /**
             * Boltzmann cooling law
             */
            boltzmann,
            /**
             * Cauchy cooling law
             */
            couchy,
            /**
             * Mixed cooling law
             */
            mixed
        };
        /**
         * Log of temperature dynamics
         */
        std::vector<double> temp_dynamic;
        std::vector<double> probs_dynamic, lambda_dynamic;

    protected:
        double min_temp_, max_temp_;
        ReductionRules reduction_rule_;
        std::function<double(double, size_t)> reduceTemperature_;
        ScheduleCorrector rebuilder_;

    public:
        /**
         * Constructor
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
         * Copy constructor
         */
        SimulatedAnnealing(const SimulatedAnnealing &other) = default;

        /**
         * Copy function for move semantics
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;
        /**
         * Set internal parameters
         */
        virtual void setParams(const ParamSet &params);
        /**
         * Get internal parameters
         */
        virtual ParamSet getParams() const;
        /**
         * Destructor
         */
        virtual ~SimulatedAnnealing() = default;

    protected:
        /**
         * Generate schedule according to the given graph
         */
        virtual Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;
        /**
         * Check if the given schedule transformation is accepted with the given temperature parameters
         */
        bool isTransitionAcceptance(double energy_delta, double temperature);
        /**
         * Set the cooling law (temperature reduction rule).
         */
        void setReductionRule(ReductionRules reduction_rule);

        /**
         * Cooling law according to Boltzmann algorithm
         */
        static double boltzmannRule(double init_temp, size_t iter_num);
        /**
         * Cooling law according to Cauchy algorithm
         */
        static double couchyRule(double init_temp, size_t iter_num);
        /**
         * Mixed cooling law
         */
        static double mixedRule(double init_temp, size_t iter_num);
    };
}
