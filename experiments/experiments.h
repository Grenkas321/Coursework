#pragma once

#include <mutex>
#include "DAGPool.h"
#include "ProgressBar.h"
#include "IterativeOptimization.h"

/**
 * Experiment orchestration API: run optimizers over DAG pools, collect results,
 * and optionally persist schedules, dynamics, and summary tables.
 */
namespace scheduling_problem::experiments
{
    /**
     * Short alias for the abstract base optimizer.
     */
    typedef algorithms::BaseOptimization BaseOptimization;

    /**
     * Short alias for iterative optimizers that expose per-iteration data.
     */
    typedef algorithms::IterativeOptimization IterativeOptimization;

    /**
     * Container of optimizers to be executed (each element owns its instance).
     */
    typedef std::vector<std::unique_ptr<BaseOptimization>> Algorithms;

    /**
     * Output/save configuration for experiment runs.
     * Controls which artifacts are produced and how directories are organized.
     */
    struct SaveParams
    {
        /**
         * What to save for a given artifact category.
         * - ALL:      save all per-run artifacts,
         * - ONLYBEST: save only the best-per-graph artifact,
         * - NONE:     do not save artifacts of this category.
         */
        enum class Mode
        {
            ALL,
            ONLYBEST,
            NONE
        };

        /** Whether to produce CSV summary tables. */
        bool make_tables;

        /** Whether to create per-algorithm output directories. */
        bool make_dirs;

        /** Save filter for iterative dynamics (cost curves, etc.). */
        Mode dynamics;

        /** Save filter for schedules (JSON dumps). */
        Mode schedules;

        /**
         * Construct a save configuration.
         *
         * @param make_tables  If true, write summary CSV tables.
         * @param make_dirs    If true, create algorithm-specific directories.
         * @param dynamics     What dynamics to save (ALL/ONLYBEST/NONE).
         * @param schedules    What schedules to save (ALL/ONLYBEST/NONE).
         */
        SaveParams(bool make_tables = true,
                   bool make_dirs = true,
                   Mode dynamics = Mode::NONE,
                   Mode schedules = Mode::NONE);
    };

    /**
     * Run a single optimizer over a batch of graphs.
     * Optionally persists per-run tables, dynamics, and schedules.
     *
     * @param batch         Batch of graphs to process.
     * @param optimizer     Optimizer instance (consumed by the call).
     * @param output_path   Base directory for all outputs of this run.
     * @param duplicates    Number of repeated runs per graph.
     * @param progress      Shared progress bar to update per run.
     * @param threads_sync  Mutex guarding shared progress/output operations.
     * @param save_params   What artifacts to save.
     */
    void runOnBatch(const additionals::DAGPool::Batch& batch,
                    std::unique_ptr<BaseOptimization>&& optimizer,
                    const std::string& output_path,
                    unsigned duplicates,
                    additionals::ProgressBar& progress,
                    std::mutex& threads_sync,
                    const SaveParams& save_params);

    /**
     * Run multiple optimizers over the entire DAG pool (batched, multithreaded).
     * Creates per-algorithm subdirectories as needed and enqueues batch jobs.
     *
     * @param algorithms    List of optimizers to execute.
     * @param dag_pool      Data pool providing batches of graphs.
     * @param output_path   Base output directory.
     * @param duplicates    Number of repeated runs per graph.
     * @param n_threads     Number of worker threads.
     * @param save_params   What artifacts to save (default: none).
     */
    void runOnPool(Algorithms& algorithms,
                   additionals::DAGPool& dag_pool,
                   const std::string& output_path,
                   unsigned duplicates,
                   unsigned n_threads,
                   const SaveParams& save_params = SaveParams());

    /**
     * Assess algorithm stability by varying the duplicate count.
     * For each value in dup_set, a separate output subtree is produced.
     *
     * @param algorithms    List of optimizers to test.
     * @param dag_pool      Data pool providing batches of graphs.
     * @param output_path   Base output directory.
     * @param dup_set       Duplicate counts to test (e.g., {1, 3, 5}).
     * @param n_threads     Number of worker threads.
     */
    void stabilityByRunsOnPool(Algorithms& algorithms,
                               additionals::DAGPool& dag_pool,
                               const std::string& output_path,
                               std::vector<unsigned> dup_set,
                               unsigned n_threads);

    /**
     * Serialize all graphs from the DAG pool to text files.
     * Each graph is written as "<path>/<graph.name()>.txt".
     *
     * @param path       Destination directory for serialized graphs.
     * @param dag_pool   Pointer to the data pool to read batches from.
     * @param n_threads  Number of worker threads (default: 1).
     */
    void makeDataSet(std::string path, additionals::DAGPool* dag_pool, unsigned n_threads = 1);
}
