#include "DAGPool.h"

namespace scheduling_problem::additionals
{
    /**
     * @brief Construct a batch container with id and reserved size.
     *
     * Initializes the underlying vector<Graph> with @p size default-constructed
     * elements and stores the @p batch_id.
     *
     * @param batch_id  Sequential batch identifier.
     * @param size      Number of graph slots to preallocate.
     */
    DAGPool::Batch::Batch(unsigned batch_id,
                          size_t size)
        : batch_id_(batch_id), std::vector<Graph>(size)
    {
    }

    /**
     * @brief Implicit conversion to unsigned as the number of graphs in the batch.
     *
     * @return Current size() of the batch.
     */
    DAGPool::Batch::operator unsigned() const
    {
        return size();
    }

    /**
     * @brief Get the identifier of this batch.
     *
     * @return Batch id assigned on construction.
     */
    unsigned DAGPool::Batch::id() const
    {
        return batch_id_;
    }

    /**
     * @brief Construct a pool controller with total samples and batch size.
     *
     * @param n_samples   Total number of graphs to generate across all batches.
     * @param batch_size  Target number of graphs per batch.
     */
    DAGPool::DAGPool(unsigned n_samples,
                     unsigned batch_size)
        : n_samples_(n_samples), batch_size_(batch_size), current_sample_(0), batch_id_(0)
    {
    }

    /**
     * @brief Desired number of graphs per batch.
     *
     * @return Batch size configured for this pool.
     */
    unsigned DAGPool::batch_size()
    {
        return batch_size_;
    }

    /**
     * @brief Total number of graphs scheduled for generation.
     *
     * @return The total sample count.
     */
    unsigned DAGPool::samplesNum()
    {
        return n_samples_;
    }
}
