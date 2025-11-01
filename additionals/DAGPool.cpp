#include "DAGPool.h"

namespace scheduling_problem::additionals
{
    DAGPool::Batch::Batch(unsigned batch_id,
                          size_t size)
        : batch_id_(batch_id), std::vector<Graph>(size)
    {
    }

    DAGPool::Batch::operator unsigned() const
    {
        return size();
    }

    unsigned DAGPool::Batch::id() const
    {
        return batch_id_;
    }

    DAGPool::DAGPool(unsigned n_samples,
                     unsigned batch_size)
        : n_samples_(n_samples), batch_size_(batch_size), current_sample_(0), batch_id_(0)
    {
    }

    unsigned DAGPool::batch_size()
    {
        return batch_size_;
    }

    unsigned DAGPool::samplesNum()
    {
        return n_samples_;
    }
}
