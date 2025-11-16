#pragma once

#include <string>
#include <vector>
#include <random>
#include "general_types.h"
#include "DAGPool.h"

namespace scheduling_problem::additionals
{
    /**
     * Implements random generation of the graph batch according to the given parameters
     */
    class GridDAGGenerator : public DAGPool
    {
    public:
        /**
         * Adjacency list as a vector of pairs
         */
        typedef std::vector<std::pair<size_t, size_t>> Adjacencies;
        /**
         * Uniform distribution of graph size
         */
        typedef std::uniform_int_distribution<size_t> uniform_size_t;
        /**
         * Uniform distribution of node weights
         */
        typedef std::uniform_int_distribution<weight_t> uniform_weight_t;
        /**
         * Grid types for the graph
         */
        enum class GridType
        {
            /**
             * Triangular grid type
             */
            TRIANGLE,
            /**
             * Rectangular grid type
             */
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
         * Constructor
         */
        GridDAGGenerator(const std::pair<size_t, size_t> &nb_columns,
                         const GridType &grid_type,
                         const std::pair<size_t, size_t> &weights,
                         unsigned n_samples,
                         unsigned batch_size,
                         unsigned seed = 42,
                         std::string prefix = "dag_");

        virtual Batch nextBatch();

        /**
         * Creates Graph instance from the given parameters
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::vector<weight_t> &weights,
                          const std::string &name);
        /**
         * Creates Graph instance from the given parameters
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::pair<size_t, size_t> &weights,
                          const std::string &name = "dag",
                          unsigned seed = 42);
        /**
         * Creates Graph instance from the given parameters
         */
        static Graph make(size_t nb_columns,
                          const GridType &grid_type,
                          const std::pair<size_t, size_t> &weights,
                          std::mt19937 &rng,
                          const std::string &name = "dag");

    private:
        /**
         * Generate a graph with triangular grid, given number of columns and given weights of nodes
         */
        static Graph makeTriangleGridDAG(size_t nb_columns,
                                         const std::vector<weight_t> &weights,
                                         const std::string &name);
        /**
         * Generate a graph with rectangular (square) grid, given number of columns and given weights of nodes
         */
        static Graph makeRectangleGridDAG(size_t nb_columns,
                                          const std::vector<weight_t> &weights,
                                          const std::string &name);
    };
}
