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
     * CSAO реализует "конвейер":
     *  1) строит стартовое решение через Greedy,
     *  2) запускает 6 SAO параллельно от текущего лучшего решения,
     *  3) выбирает лучшее и подаёт его в следующую волну,
     *  4) останавливается по saturation (число волн без улучшений).
     *
     * Seed в стохастических частях генерируется заново при каждом запуске алгоритма.
     *
     * Параметры разбиения/подобластей оставлены для будущих расширений.
     */
    class ConcurrentSAO : public BaseOptimization
    {
    public:
        typedef SimulatedAnnealing::ReductionRules ReductionRules;
        typedef std::vector<std::vector<bool>> AdjacencyMatrix;

        ConcurrentSAO(unsigned partitions_count = 0,
                      unsigned rsearch_iters = 0,
                      bool warm_start = false,
                      bool subareas = false,
                      double min_temp = 0.1,
                      double max_temp = 10.0,
                      ReductionRules reduction_rule = ReductionRules::boltzmann,
                      unsigned saturation = 10,
                      double improvement = 0.0,
                      unsigned seed = 42,
                      const std::string &label = "csao");

        virtual Schedule schedule_(const Graph &graph) override;

        std::unique_ptr<BaseOptimization> copy() const override;

        ParamSet getParams() const override;
        void setParams(const ParamSet &params) override;

        std::vector<std::pair<unsigned, unsigned>> conveyor;

    private:
        static AdjacencyMatrix makeAdjacencyMatrix(const Graph &graph);
        std::pair<size_t, size_t> subarea(const AdjacencyMatrix &adjacency_matrix);
        std::vector<Graph> makePartitions(const Graph &graph);

    private:
        unsigned partitions_count_;
        unsigned rsearch_iters_;
        bool warm_start_;
        bool subareas_;

        randgen rng_;
        ParamSet algo_params_;
    };
}
