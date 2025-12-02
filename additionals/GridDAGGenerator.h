#pragma once

#include <string>
#include <vector>
#include <random>
#include "general_types.h"
#include "DAGPool.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Random generator of DAGs with grid topology.
     *
     * Produces batches of graphs whose structure follows either a triangular
     * or rectangular grid pattern. Vertex weights are sampled from a range.
     */
    class GridDAGGenerator : public DAGPool
    {
    public:
        /** @brief Adjacency list represented as (source,target) pairs. */
        typedef std::vector<std::pair<size_t, size_t>> Adjacencies;
        /** @brief Integer uniform distribution for grid size parameters. */
        typedef std::uniform_int_distribution<size_t> uniform_size_t;
        /** @brief Integer uniform distribution for vertex weights. */
        typedef std::uniform_int_distribution<weight_t> uniform_weight_t;

        /**
         * @brief Grid topology type.
         */
        enum class GridType
        {
            /** Triangular grid (each node feeds two successors on the next column when possible). */
            TRIANGLE,
            /** Rectangular (square) grid with regular forward connections. */
            RECTANGLE
        };

    private:
        uniform_size_t nb_columns_;
        GridType grid_type_;
        std::string prefix_;
        std::mt19937 rng_;
        uniform_weight_t weights_;

    public:
        /**
         * @brief Construct a grid-based DAG generator.
         *
         * @param nb_columns  Inclusive range for number of columns.
         * @param grid_type   Grid topology (TRIANGLE or RECTANGLE).
         * @param weights     Inclusive range for vertex weights (min,max).
         * @param n_samples   Total number of graphs to generate.
         * @param batch_size  Number of graphs returned per nextBatch() call.
         * @param seed        RNG seed.
         * @param prefix      Name prefix for generated graphs.
         */
        GridDAGGenerator(const std::pair<size_t, size_t> &nb_columns,
                         const GridType &grid_type,
                         const std::pair<size_t, size_t> &weights,
                         unsigned n_samples,
                         unsigned batch_size,
                         unsigned seed = 42,
                         std::string prefix = "dag_");

        /**
         * @brief Produce the next batch of generated graphs.
         *
         * @return Batch containing graphs and a batch identifier.
         */
        virtual Batch nextBatch();

        /**
         * @brief Build a grid DAG with fixed per-vertex weights.
         *
         * Dispatches to the corresponding grid constructor based on @p grid_type.
         *
         * @param nb_columns Number of grid columns.
         * @param grid_type  Grid topology.
         * @param weights    Per-vertex weights; size must match the number of vertices implied by the grid.
         * @param name       Graph name.
         * @return Generated DAG.
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::vector<weight_t> &weights,
                          const std::string &name);

        /**
         * @brief Build a grid DAG sampling vertex weights from a range.
         *
         * @param nb_columns Number of grid columns.
         * @param grid_type  Grid topology.
         * @param weights    Inclusive range for vertex weights.
         * @param name       Graph name.
         * @param seed       RNG seed used internally.
         * @return Generated DAG.
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::pair<size_t, size_t> &weights,
                          const std::string &name = "dag",
                          unsigned seed = 42);

        /**
         * @brief Build a grid DAG sampling vertex weights from a range using an external RNG.
         *
         * @param nb_columns Number of grid columns.
         * @param grid_type  Grid topology.
         * @param weights    Inclusive range for vertex weights.
         * @param rng        RNG instance to use.
         * @param name       Graph name.
         * @return Generated DAG.
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::pair<size_t, size_t> &weights,
                          std::mt19937 &rng,
                          const std::string &name = "dag");

    private:
        /**
         * @brief Construct a triangular-grid DAG.
         *
         * Lays out vertices in @p nb_columns columns and connects each vertex to forward
         * successors forming a triangular pattern. Assigns per-vertex weights from @p weights.
         *
         * @param nb_columns Number of grid columns.
         * @param weights    Per-vertex weights; size must match vertex count in the layout.
         * @param name       Graph name.
         * @return Generated DAG.
         */
        static Graph makeTriangleGridDAG(size_t nb_columns,
                                         const std::vector<weight_t> &weights,
                                         const std::string &name);

        /**
         * @brief Construct a rectangular-grid DAG.
         *
         * Lays out vertices in @p nb_columns columns with rectangular connectivity
         * (forward edges to next column according to the pattern). Assigns per-vertex
         * weights from @p weights.
         *
         * @param nb_columns Number of grid columns.
         * @param weights    Per-vertex weights; size must match vertex count in the layout.
         * @param name       Graph name.
         * @return Generated DAG.
         */
        static Graph makeRectangleGridDAG(size_t nb_columns,
                                          const std::vector<weight_t> &weights,
                                          const std::string &name);
    };
}
