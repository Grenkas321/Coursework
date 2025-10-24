#pragma once

#include <random>
#include "BaseOptimization.h"
#include "Greedy.h"

namespace scheduling_problem::algorithms
{

    extern Greedy BASELINE;

    /**
     * Random generator defined
     */
    typedef std::mt19937 randgen;

    /**
     * @brief Base class for iterative algorithms
     */
    class IterativeOptimization : public BaseOptimization
    {

    protected:
        unsigned iters_count_, saturation_, seed_;
        double improvement_;
        randgen rng_;
        std::vector<scheduling_problem::weight_t> cost_dynamics_;
        std::unique_ptr<BaseOptimization> baseline_;

    public:
        /**
         * Constructor
         * @param baseline Algorithm for making baseline (initial) solution
         * @param saturation Maximal number of iterations without solution improvement
         * @param improvement Improvement percentage bound for improvement recognition
         * @param seed Seed for random generators
         * @param label Algorithm name
         */
        IterativeOptimization(const BaseOptimization &baseline = BASELINE,
                              unsigned saturation = 0,
                              double improvement = 0,
                              unsigned seed = 42,
                              const std::string &label = "iterative");
        /**
         * Copy constructor
         */
        IterativeOptimization(const IterativeOptimization &other) = default;

        /**
         * Create schedule from base schedule
         * @param graph Input graph
         * @param base_schedule Baseline solution
         * @return Constructed schedule
         */
        Schedule schedule(const Graph &graph, const Schedule &base_schedule);

        /**
         * Set algorithm parameters
         * @param params algorithm parameters
         */
        virtual void setParams(const ParamSet &params);

        /**
         * Get algorithm parameters
         * @return Set of algorithm parameters
         */
        virtual ParamSet getParams() const;

        /**
         * Copy function for the class
         * @return std::unique_ptr<BaseOptimization>
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override = 0;

        /**
         * Iterations counter
         * @return Number of iterations already performed
         */
        unsigned itersCount() const;
        /**
         * Get the vector of cost on each algorithm step
         */
        std::vector<scheduling_problem::weight_t> costDynamics() const;

    protected:
        /**
         * Constructs a %schedule for a given graph
         * @param graph Input graph
         * @return Constructed schedule
         */
        virtual Schedule schedule_(const Graph &graph);

        /**
         * Constructs a schedule for a given graph and baseline schedule
         * @param graph Input graph
         * @param base_schedule Baseline solution
         * @return Constructed schedule
         */
        virtual Schedule schedule_(const Graph &graph, const Schedule &base_schedule) = 0;
    };
}
