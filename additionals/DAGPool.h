#pragma once

#include <vector>
#include "general_types.h"

namespace scheduling_problem::additionals
{
    /**
     * Stores generated graphs
     */
    class DAGPool
    {
    public:
        /**
         * Stores %Graph instances, used for looping over graph batch
         */
        class Batch : public std::vector<Graph>
        {
            /*
             * Simple vector contains Graph instances. This class has unsigned()
             * method (special for while-looping).
             */

        protected:
            unsigned batch_id_;

        public:
            /**
             * Get batch by id
             */
            Batch(unsigned batch_id = 0, size_t size = 0);

            /**
             * To unsigned value
             */
            operator unsigned() const;

            /**
             * Get pool id
             */
            unsigned id() const;
        };

    protected:
        unsigned batch_size_, n_samples_, current_sample_, batch_id_;

    public:
        /**
         * Class constructor
         */
        DAGPool(unsigned n_samples,
                unsigned batch_size);

        /**
         * Get next batch
         */
        virtual Batch nextBatch() = 0;

        /**
         * Get batch size
         */
        unsigned batch_size();

        /**
         * Get number of graphs total
         */
        unsigned samplesNum();
    };
}
