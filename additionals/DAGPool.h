#pragma once

#include <vector>
#include "general_types.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Container and interface for batched generation/storage of graphs.
     *
     * Manages iteration over generated graphs by fixed-size batches and
     * exposes metadata such as total sample count and batch size.
     */
    class DAGPool
    {
    public:
        /**
         * @brief Batch of graphs with an associated identifier.
         *
         * Inherits from std::vector<Graph> and adds a lightweight
         * identifier plus an implicit conversion to unsigned for
         * convenient while-loop usage.
         */
        class Batch : public std::vector<Graph>
        {
        protected:
            /** @brief Sequential id of this batch. */
            unsigned batch_id_;

        public:
            /**
             * @brief Construct a batch with id and reserved size.
             *
             * Initializes the underlying vector with @p size default-constructed
             * Graph entries.
             *
             * @param batch_id  Identifier of the batch.
             * @param size      Number of graph slots to preallocate.
             */
            Batch(unsigned batch_id = 0, size_t size = 0);

            /**
             * @brief Implicit conversion to the number of graphs in the batch.
             * @return Current size of the batch.
             */
            operator unsigned() const;

            /**
             * @brief Get the identifier of this batch.
             * @return The batch id.
             */
            unsigned id() const;
        };

    protected:
        /** @brief Target graphs per batch. */
        unsigned batch_size_;
        /** @brief Total graphs to be produced across all batches. */
        unsigned n_samples_;
        /** @brief Number of graphs already produced. */
        unsigned current_sample_;
        /** @brief Counter for batches produced so far (next id to assign). */
        unsigned batch_id_;

    public:
        /**
         * @brief Construct a pool with total sample count and batch size.
         *
         * @param n_samples   Total number of graphs to generate.
         * @param batch_size  Desired number of graphs per batch.
         */
        DAGPool(unsigned n_samples,
                unsigned batch_size);

        /**
         * @brief Produce the next batch of graphs.
         *
         * Implementations should generate up to @p batch_size_ graphs (or fewer
         * if @p n_samples_ would be exceeded), assign an id, and return them.
         *
         * @return The next batch of graphs.
         */
        virtual Batch nextBatch() = 0;

        /**
         * @brief Get the configured batch size.
         * @return Number of graphs per batch.
         */
        unsigned batch_size();

        /**
         * @brief Get the total number of graphs scheduled for generation.
         * @return Total sample count.
         */
        unsigned samplesNum();
    };
}
