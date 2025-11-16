#pragma once

#include <mutex>
#include "DAGPool.h"
#include "ProgressBar.h"
#include "IterativeOptimization.h"

/**
 * Stores methods for proper experimental evaluation
*/
namespace scheduling_problem::experiments
{
    /**
     * Base optimization, shortened
    */
    typedef algorithms::BaseOptimization BaseOptimization;
    /**
     * Iterative optimization, shortened
    */
    typedef algorithms::IterativeOptimization IterativeOptimization;
    /**
     * Vector of the given optimizers
    */
    typedef std::vector<std::unique_ptr<BaseOptimization>> Algorithms;

    /**
     * Saves parameters of the experiments
    */
    struct SaveParams
    {
        /**
         * Modes for saving parameters
        */
        enum class Mode
        {
            /**
             * Save all parameters to file
            */
            ALL, 
            /**
             * Save only best parameters to file
            */
            ONLYBEST, 
            /**
             * Do not save any parameters to file
            */
            NONE
        };

        /**
         * Flag to create a table with summary data
        */
        bool make_tables;
        /**
         * Flag to create directories for output data
        */
        bool make_dirs;
        /**
         * Filter for saving the computation dynamics
        */
        Mode dynamics;
        /**
         * Filter for saving schedule data
        */
        Mode schedules;

        SaveParams(bool make_tables = true,
                   bool make_dirs = true,
                   Mode dynamics = Mode::NONE,
                   Mode schedules = Mode::NONE);
    };

    void runOnBatch(const additionals::DAGPool::Batch& batch,
                    std::unique_ptr<BaseOptimization>&& optimizer,
                    const std::string& output_path,
                    unsigned duplicates,
                    additionals::ProgressBar& progress,
                    std::mutex& threads_sync,
                    const SaveParams& save_params);

    void runOnPool(Algorithms& algorithms,
                   additionals::DAGPool& dag_pool,
                   const std::string& output_path,
                   unsigned duplicates,
                   unsigned n_threads,
                   const SaveParams& save_params = SaveParams());

    void stabilityByRunsOnPool(Algorithms& algorithms,
                               additionals::DAGPool& dag_pool,
                               const std::string& output_path,
                               std::vector<unsigned> dup_set,
                               unsigned n_threads);

    void makeDataSet(std::string path, additionals::DAGPool* dag_pool, unsigned n_threads = 1);
}
