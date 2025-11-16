#pragma once

#include <tuple>
#include <vector>
#include <random>
#include <unordered_set>
#include "general_types.h"
#include "DAGPool.h"

/**
 *  Stores auxiliary functions for graph input\output that are not considered as a part of scheduling algorithm
 */
namespace scheduling_problem::additionals
{
    /**
     * Generates batch of graphs according to the given set of generation parameters
     */
    class DAGGenerator : public DAGPool
    {
    private:
        typedef std::tuple<weight_t, weight_t, std::vector<weight_t>> GraphParams;

        typedef std::vector<std::pair<weight_t, weight_t>> Adjacencies;

        typedef std::uniform_int_distribution<weight_t> uniform_weight_t;

        std::mt19937 rng_;
        std::string prefix_;
        std::vector<std::pair<weight_t, weight_t>> vertedge_map_;
        std::uniform_int_distribution<weight_t> weights_;

    public:
        /**
         * Class constructor
         */
        DAGGenerator(const std::vector<std::pair<weight_t, weight_t>> &vertedge_map,
                     const std::pair<weight_t, weight_t> &weights,
                     unsigned n_samples,
                     unsigned batch_size,
                     unsigned seed = 42,
                     std::string prefix = "dag_");

        /**
         * Get next batch
         */
        virtual Batch nextBatch();

        static Graph make(weight_t n_vertex,
                          weight_t n_edges,
                          const std::vector<weight_t> &weights,
                          const std::string &name,
                          std::mt19937 &rng);

        static Graph make(weight_t n_vertex,
                          weight_t n_edges,
                          const std::pair<weight_t, weight_t> &weights,
                          const std::string &name = "dag",
                          unsigned seed = 42);

    private:
        GraphParams generateGraphParams();

        static void makeConnectedComponent(weight_t n_vertex,
                                           Adjacencies &adjacencies,
                                           std::unordered_set<weight_t> &connections,
                                           std::mt19937 &rng);
    };
}
