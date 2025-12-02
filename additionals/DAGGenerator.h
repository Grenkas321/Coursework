#pragma once

#include <tuple>
#include <vector>
#include <random>
#include <unordered_set>
#include "general_types.h"
#include "DAGPool.h"

/**
 * @brief Utilities for generating random DAGs for experiments (not part of the core scheduling).
 */
namespace scheduling_problem::additionals
{
    /**
     * @brief Generates batches of random DAGs according to configured size/weight ranges.
     */
    class DAGGenerator : public DAGPool
    {
    private:
        /** @brief Tuple of (n_vertices, n_edges, per-vertex weights). */
        typedef std::tuple<weight_t, weight_t, std::vector<weight_t>> GraphParams;

        /** @brief List of directed edges represented as (source,target) pairs. */
        typedef std::vector<std::pair<weight_t, weight_t>> Adjacencies;

        /** @brief Integer uniform distribution over weight_t. */
        typedef std::uniform_int_distribution<weight_t> uniform_weight_t;

        std::mt19937 rng_;
        std::string prefix_;
        std::vector<std::pair<weight_t, weight_t>> vertedge_map_;
        std::uniform_int_distribution<weight_t> weights_;

    public:
        /**
         * @brief Construct a generator with ranges for (V,E) and vertex weights.
         *
         * @param vertedge_map  Allowed (n_vertices, n_edges) pairs to sample from.
         * @param weights       Inclusive range for vertex weights (min, max).
         * @param n_samples     Total number of graphs to produce.
         * @param batch_size    Number of graphs returned per nextBatch() call.
         * @param seed          RNG seed.
         * @param prefix        Name prefix for generated graphs.
         */
        DAGGenerator(const std::vector<std::pair<weight_t, weight_t>> &vertedge_map,
                     const std::pair<weight_t, weight_t> &weights,
                     unsigned n_samples,
                     unsigned batch_size,
                     unsigned seed = 42,
                     std::string prefix = "dag_");

        /**
         * @brief Produce the next batch of graphs.
         *
         * Continues generation until either batch_size or total n_samples is reached.
         *
         * @return Batch container with generated graphs and batch id.
         */
        virtual Batch nextBatch();

        /**
         * @brief Build a DAG with fixed per-vertex weights.
         *
         * Creates a connected backbone of V-1 forward edges, then adds extra forward
         * edges uniformly without duplicates until E edges are present. Weights are
         * assigned from the provided vector.
         *
         * @param n_vertex  Number of vertices V.
         * @param n_edges   Number of edges E.
         * @param weights   Vector of vertex weights of size V.
         * @param name      Graph name.
         * @param rng       RNG to use.
         * @return Generated DAG, or an empty graph if parameters are invalid.
         */
        static Graph make(weight_t n_vertex,
                          weight_t n_edges,
                          const std::vector<weight_t> &weights,
                          const std::string &name,
                          std::mt19937 &rng);

        /**
         * @brief Build a DAG with vertex weights sampled uniformly from a range.
         *
         * @param n_vertex  Number of vertices V.
         * @param n_edges   Number of edges E.
         * @param weights   Inclusive integer range for vertex weights.
         * @param name      Graph name.
         * @param seed      RNG seed (used internally to construct the RNG).
         * @return Generated DAG, or an empty graph if parameters are invalid.
         */
        static Graph make(weight_t n_vertex,
                          weight_t n_edges,
                          const std::pair<weight_t, weight_t> &weights,
                          const std::string &name = "dag",
                          unsigned seed = 42);

    private:
        /**
         * @brief Sample (V,E,weights) triple from configured distributions.
         *
         * @return Tuple {n_vertex, n_edges, weights_vector}.
         */
        GraphParams generateGraphParams();

        /**
         * @brief Create a connected backbone of V-1 forward edges.
         *
         * Randomly grows a connected component by linking a chosen connected
         * vertex to a chosen unconnected vertex with source < target.
         * Fills adjacencies[0..V-2] and records encoded pairs in connections.
         *
         * @param n_vertex     Number of vertices V.
         * @param adjacencies  Output edge list (size >= V-1) to store tree edges.
         * @param connections  Set of encoded pairs source*V + target to avoid duplicates.
         * @param rng          RNG to use.
         */
        static void makeConnectedComponent(weight_t n_vertex,
                                           Adjacencies &adjacencies,
                                           std::unordered_set<weight_t> &connections,
                                           std::mt19937 &rng);
    };
}
