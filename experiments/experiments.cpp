#include "experiments.h"

#include <filesystem>

#include "ConcurrentSAO.h"
#include "ThreadPool.h"
#include "DataFrame.h"
#include "additionals.h"

namespace scheduling_problem::experiments
{
#define STANDARD_BAR_LEN 60

    /**
     * Construct save/output configuration for experiment runs.
     *
     * @param make_tables  If true, write per-graph CSV tables.
     * @param make_dirs    If true, create per-algorithm subdirectories.
     * @param dynamics     What dynamics to save (NONE | BEST | ALL).
     * @param schedules    What schedules to save (NONE | BEST | ALL).
     */
    SaveParams::SaveParams(bool make_tables,
                           bool make_dirs,
                           Mode dynamics,
                           Mode schedules)
                           : make_tables(make_tables)
                           , make_dirs(make_dirs)
                           , dynamics(dynamics)
                           , schedules(schedules)
    {
    }

    /**
     * Build a list of DataFrame column names for the selected optimizer and number of runs.
     * For iterative optimizers, an "iters_<k>" column is also added per run.
     *
     * @param optimizer   Optimizer instance (may be iterative).
     * @param duplicates  Number of repeated runs per graph.
     * @return Vector of column names.
     */
    std::vector<std::string> makeDFColumns(std::unique_ptr<BaseOptimization>& optimizer, const unsigned& duplicates)
    {
        bool is_iterative = dynamic_cast<IterativeOptimization*>(optimizer.get()) ? true : false;
        std::vector<std::string> columns{ "n_vertex", "n_edges" };
        for (unsigned run_step(1); run_step <= duplicates; run_step++) {
            auto step = std::to_string(run_step);
            columns.insert(columns.end(), { "cost_" + step, "time_" + step });
            if (is_iterative)
                columns.push_back("iters_" + step);
        }

        return columns;
    }

    /**
     * Run a single optimizer on a batch of graphs.
     * Records best solutions and optional dynamics/schedules per graph, and updates a shared progress bar.
     *
     * @param batch         Batch of graphs to process.
     * @param optimizer     Optimizer instance (consumed by this function).
     * @param output_path   Base path for all outputs of this optimizer and batch.
     * @param duplicates    Number of repeated runs per graph.
     * @param progress      Shared progress bar to update after each run.
     * @param threads_sync  Mutex guarding progress bar and shared outputs.
     * @param save_params   What to save (tables, dynamics, schedules).
     */
    void runOnBatch(const additionals::DAGPool::Batch& batch,
                    std::unique_ptr<BaseOptimization>&& optimizer,
                    const std::string& output_path,
                    unsigned duplicates,
                    additionals::ProgressBar& progress,
                    std::mutex& threads_sync,
                    const SaveParams& save_params)
    {
        auto columns = makeDFColumns(optimizer, duplicates);
        additionals::DataFrame<std::string, std::string, size_t> dataframe(columns);
        bool is_iterative = dynamic_cast<IterativeOptimization*>(optimizer.get()) ? 1 : 0;
        for (auto& graph : batch) {
            Schedule best_solution;
            std::vector<weight_t> best_dynamics;
            std::vector<double> best_temp_dynamic, best_probs_dynamic, best_lambda_dynamic;
            std::vector<std::vector<weight_t>> best_conveyor;
            std::string save_schedule_dir, save_dynamics_dir;
            if (save_params.schedules == SaveParams::Mode::ALL) {
                save_schedule_dir = output_path + "/schedules/" + graph.name();
                std::filesystem::create_directory(save_schedule_dir);
            }
            if (is_iterative && save_params.dynamics == SaveParams::Mode::ALL) {
                save_dynamics_dir = output_path + "/dynamics/" + graph.name();
                std::filesystem::create_directory(save_dynamics_dir);
            }
            additionals::DataFrame<std::string, std::string, size_t>::Row row{
                { "n_vertex", boost::num_vertices(graph) },
                { "n_edges", boost::num_edges(graph) }
            };

            for (unsigned step(1); step <= duplicates; step++) {
                auto solution = optimizer->schedule(graph);
                if (!best_solution.size() || best_solution.cost() > solution.cost()) {
                    best_solution = solution;
                    if (is_iterative) {
                        auto iterative_opt = dynamic_cast<IterativeOptimization*>(optimizer.get());
                        best_dynamics = iterative_opt->costDynamics();
                        if (dynamic_cast<algorithms::SimulatedAnnealing*>(iterative_opt)) {
                            best_temp_dynamic = dynamic_cast<algorithms::SimulatedAnnealing*>(iterative_opt)->temp_dynamic;
                            best_probs_dynamic = dynamic_cast<algorithms::SimulatedAnnealing*>(iterative_opt)->probs_dynamic;
                            best_lambda_dynamic = dynamic_cast<algorithms::SimulatedAnnealing*>(iterative_opt)->lambda_dynamic;
                        }
                    }
                    if (dynamic_cast<algorithms::ConcurrentSAO*>(optimizer.get())) {
                        best_conveyor = dynamic_cast<algorithms::ConcurrentSAO*>(optimizer.get())->conveyor;
                    }
                }

                {
                    auto strstep = std::to_string(step);
                    if (save_params.make_tables) {
                        row["cost_" + strstep] = solution.cost();
                        row["time_" + strstep] = optimizer->duration();
                        if (is_iterative)
                            row["iters_" + strstep] = dynamic_cast<IterativeOptimization*>(optimizer.get())->itersCount();
                    }

                    if (is_iterative && save_dynamics_dir.size()) {
                        auto dynamics = dynamic_cast<IterativeOptimization*>(optimizer.get())->costDynamics();
                        std::ofstream file(save_dynamics_dir + "/step_" + strstep + ".txt");
                        std::copy(dynamics.begin(), dynamics.end(), std::ostream_iterator<weight_t>({ file, " " }));
                    }

                    if (save_schedule_dir.size())
                        solution.dump(save_schedule_dir + "/step_" + strstep + ".json");
                }

                {
                    std::lock_guard<std::mutex> lock(threads_sync);
                    progress.show();
                }
            }

            {
                if (save_params.make_tables)
                    dataframe.append({ graph.name(), row });

                if (save_params.schedules != SaveParams::Mode::NONE)
                    best_solution.dump(output_path + "/schedules/best/" + graph.name() + ".json");

                if (is_iterative && save_params.dynamics != SaveParams::Mode::NONE) {
                    std::ofstream file(output_path + "/dynamics/best/" + graph.name() + ".txt");
                    std::copy(best_dynamics.begin(), best_dynamics.end(), std::ostream_iterator<weight_t>({ file, " " }));
                }

                if (best_temp_dynamic.size()) {
                    {
                        std::ofstream file(output_path + "/temp_dynamics/best/" + graph.name() + ".txt");
                        std::copy(best_temp_dynamic.begin(), best_temp_dynamic.end(), std::ostream_iterator<double>({ file, " " }));
                    }
                    {
                        std::ofstream file(output_path + "/prob_dynamics/best/" + graph.name() + ".txt");
                        std::copy(best_probs_dynamic.begin(), best_probs_dynamic.end(), std::ostream_iterator<double>({ file, " " }));
                    }
                    {
                        std::ofstream file(output_path + "/lambda_dynamics/best/" + graph.name() + ".txt");
                        std::copy(best_lambda_dynamic.begin(), best_lambda_dynamic.end(), std::ostream_iterator<double>({ file, " " }));
                    }
                }

                if (best_conveyor.size()) {
                    std::ofstream file(output_path + "/conveyor/best/" + graph.name() + ".txt");
                    for (auto& stage : best_conveyor) {
                        std::copy(stage.begin(), stage.end(), std::ostream_iterator<weight_t>(file, " "));
                        file << std::endl;
                    }
                }
            }
        }

        if (save_params.make_tables) {
            std::string dumppath = output_path + "/table_data/" + optimizer->label()
                                   + "_batch_" + std::to_string(batch.id()) + ".csv";
            dataframe.toCsv(dumppath);
        }
    }

    /**
     * Run a set of algorithms over a whole DAG pool with multithreading.
     * Creates algorithm-specific output directories and enqueues batch jobs.
     *
     * @param algorithms   List of algorithm instances to run.
     * @param dag_pool     Data pool to iterate by batches.
     * @param output_path  Base output directory.
     * @param duplicates   Number of repeated runs per graph.
     * @param n_threads    Number of worker threads in the pool.
     * @param save_params  What to save (tables, dynamics, schedules).
     */
    void runOnPool(Algorithms& algorithms,
                   additionals::DAGPool& dag_pool,
                   const std::string& output_path,
                   unsigned duplicates,
                   unsigned n_threads,
                   const SaveParams& save_params)
    {
        additionals::DAGPool::Batch graphs;
        additionals::ThreadPool thread_pool(n_threads);
        unsigned common_iters = dag_pool.samplesNum() * duplicates * algorithms.size();
        additionals::ProgressBar progress(STANDARD_BAR_LEN, common_iters);
        std::mutex threads_sync;
        while ((graphs = dag_pool.nextBatch())) {
            for (auto& alg : algorithms) {
                std::string alg_output_path(output_path);
                if (save_params.make_dirs) {
                    alg_output_path += "/" + alg->label();
                    std::filesystem::create_directory(alg_output_path);
                }

                if (save_params.make_tables)
                    std::filesystem::create_directory(alg_output_path + "/table_data");
                if (save_params.dynamics != SaveParams::Mode::NONE)
                    std::filesystem::create_directories(alg_output_path + "/dynamics/best");
                if (save_params.schedules != SaveParams::Mode::NONE)
                    std::filesystem::create_directories(alg_output_path + "/schedules/best");
                if (dynamic_cast<algorithms::SimulatedAnnealing*>(alg.get())) {
                    std::filesystem::create_directories(alg_output_path + "/temp_dynamics/best");
                    std::filesystem::create_directories(alg_output_path + "/prob_dynamics/best");
                    std::filesystem::create_directories(alg_output_path + "/lambda_dynamics/best");
                }
                if (dynamic_cast<algorithms::ConcurrentSAO*>(alg.get())) {
                    std::filesystem::create_directories(alg_output_path + "/conveyor/best");
                }
                        runOnBatch(graphs, alg->copy(), alg_output_path,
                            duplicates, progress, threads_sync,save_params);
            }
        }
    }

    /**
     * Assess algorithm stability by running multiple duplicate-count settings.
     * For each value in dup_set, creates a separate output subtree and runs the batch.
     *
     * @param algorithms   List of algorithms to test.
     * @param dag_pool     Data pool to iterate by batches.
     * @param output_path  Base output directory.
     * @param dup_set      List of duplicate counts to test, e.g., {1, 3, 5}.
     * @param n_threads    Number of worker threads.
     */
    void stabilityByRunsOnPool(Algorithms& algorithms,
                               additionals::DAGPool& dag_pool,
                               const std::string& output_path,
                               std::vector<unsigned> dup_set,
                               unsigned n_threads)
    {
        additionals::DAGPool::Batch batch;
        additionals::ThreadPool thread_pool(n_threads);
        std::mutex thread_sync;
        unsigned common_dups = std::accumulate(dup_set.begin(), dup_set.end(), 0);
        unsigned common_iters = dag_pool.samplesNum() * common_dups * algorithms.size();
        additionals::ProgressBar progress(STANDARD_BAR_LEN, common_iters);
        SaveParams config;
        while ((batch = dag_pool.nextBatch()))
            for (auto& alg : algorithms) {
                std::string alg_output_path(output_path + "/" + alg->label());
                std::filesystem::create_directory(alg_output_path);

                for (auto duplicates : dup_set) {
                    auto alg_dup_output(alg_output_path + "/dups_" + std::to_string(duplicates));
                    std::filesystem::create_directories(alg_dup_output + "/table_data");
                    thread_pool.enqueue([batch, alg_dup_output, &alg, &thread_sync, &progress, duplicates, config]
                        {
                            runOnBatch(batch, alg->copy(), alg_dup_output,
                                duplicates, progress, thread_sync, config);
                        }
                    );
                }
            }
    }

    /**
     * Serialize and write every graph from the DAG pool into files.
     * Each graph is saved as "<path>/<graph.name()>.txt".
     *
     * @param path       Destination directory for serialized graphs.
     * @param dag_pool   Pointer to the graph pool to read batches from.
     * @param n_threads  Number of worker threads for parallel writing.
     */
    void makeDataSet(std::string path, additionals::DAGPool* dag_pool, unsigned n_threads)
    {
        additionals::DAGPool::Batch graphs;
        additionals::ThreadPool pool(n_threads);
        while ((graphs = dag_pool->nextBatch())) {
            pool.enqueue([graphs, &path]()
                {
                    for (auto graph : graphs)
                        additionals::serializeGraph(graph, path + "/" + graph.name() + ".txt");
                }
            );
        }
    }
}
