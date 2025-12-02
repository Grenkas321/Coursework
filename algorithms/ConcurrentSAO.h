#pragma once

#include <vector>
#include <future>
#include <thread>
#include "SimulatedAnnealing.h"

namespace scheduling_problem::algorithms
{
    /**
     * Implements simulated annealing in concurrent manner.
     *
     * В текущей версии schedule_ просто оборачивает обычный SimulatedAnnealing,
     * чтобы результаты CSAO совпадали с SAO. Параметры хранения/разбиения
     * остаются, но в schedule_ пока не используются.
     */
    class ConcurrentSAO : public BaseOptimization
    {
    public:
        /**
         * Cooling laws (temperature reduction rules).
         */
        typedef SimulatedAnnealing::ReductionRules ReductionRules;

        /**
         * Adjacency matrix for the task graph.
         */
        typedef std::vector<std::vector<bool>> AdjacencyMatrix;

    private:
        unsigned partitions_count_;
        unsigned rsearch_iters_;
        bool warm_start_;
        bool subareas_;
        randgen rng_;
        ParamSet algo_params_;

    public:
        /**
         * History of costs (для отрисовки графиков и анализа).
         */
        std::vector<std::vector<weight_t>> conveyor;

        /**
         * Constructor.
         */
        ConcurrentSAO(unsigned partitions_count = std::thread::hardware_concurrency(),
                      unsigned rsearch_iters = 1000,
                      bool warm_start = true,
                      bool subareas = true,
                      double min_temp = 1,
                      double max_temp = 13,
                      ReductionRules reduction_rule = ReductionRules::boltzmann,
                      unsigned saturation = 0,
                      double improvement = 0,
                      unsigned seed = 42,
                      const std::string &label = "csao");

        /**
         * Copy function.
         */
        virtual std::unique_ptr<BaseOptimization> copy() const override;

        /**
         * Get the internal algorithm parameters.
         */
        virtual ParamSet getParams() const override;

        /**
         * Sets internal parameters.
         */
        virtual void setParams(const ParamSet &params) override;

        /**
         * Create partitions on a given graph and return vector of subgraphs
         * corresponding to partitions.
         *
         * (Сейчас в schedule_ не используется, оставлено на будущее.)
         */
        std::vector<Graph> makePartitions(const Graph &graph);

    protected:
        /**
         * Constructs a schedule for a given graph.
         */
        virtual Schedule schedule_(const Graph &graph) override;

        /**
         * Create adjacency matrix from the given graph.
         */
        AdjacencyMatrix makeAdjacencyMatrix(const Graph &graph);

        /**
         * Find the subarea according to adjacency matrix.
         */
        std::pair<size_t, size_t> subarea(const AdjacencyMatrix &adjacency_matrix);
    };
}
