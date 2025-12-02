#include <vector>
#include <tuple>
#include <memory>
#include "SimulatedAnnealing.h"
#include "AntColonySystem.h"
#include "SeriesParallel.h"
#include "general_types.h"
#include "additionals.h"
#include "parameter.h"
#include "DAGReader.h"
#include "DAGGenerator.h"
#include "json.hpp"
#include <boost/program_options.hpp>

/**
 * Command-line parsing helpers and types for configuring experiments.
 */
namespace parse
{
    using namespace scheduling_problem;
    using namespace scheduling_problem::additionals;
    using namespace scheduling_problem::algorithms;
    namespace bpo = boost::program_options;

    /**
     * Container of instantiated optimization algorithms.
     * Each element owns a concrete optimizer (e.g., SA, ACS, etc.).
     */
    typedef std::vector<std::unique_ptr<BaseOptimization>> Algorithms;

    /**
     * Parameter grid for sweeps:
     * algorithm_name -> (parameter_name -> list of values).
     */
    typedef std::unordered_map<std::string, std::unordered_map<std::string, std::vector<parameter>>> Grid;

    /**
     * High-level action requested from the CLI.
     */
    enum class Command
    {
        /** Execute selected algorithms on provided data. */
        RUN,
        /** Re-run algorithms multiple times to assess stability. */
        STABILITY,
        /** Build and dump a single schedule for one graph. */
        SCHEDULE
    };

    /**
     * Parsed command-line parameters and resolved runtime objects.
     */
    struct ParsedParams
    {
        /** Action to perform. */
        Command command;

        /** Algorithms to run (possibly multiple, in order). */
        Algorithms algorithms;

        /** Input dataset (graphs) resolved from CLI flags. */
        std::unique_ptr<DAGPool> data;

        /** Output directory for results, logs, and artifacts. */
        std::string output_path;

        /** Number of dataset duplicates to generate or use. */
        unsigned duplicates;

        /** Number of worker threads for parallel experiments. */
        unsigned n_threads;

        /**
         * Per-duplicate repeat counts: for each duplicate, how many
         * times to re-run the algorithm on the same data.
         */
        std::vector<unsigned> dupset;

        /** Hyperparameter grid for sweeps, keyed by algorithm and parameter. */
        Grid grid;
    };

    /**
     * Parse command-line arguments and build a ready-to-run configuration.
     *
     * The function interprets common flags such as algorithm selection,
     * input data source, output directory, threading, duplicates, stability
     * repeats, and parameter grids. It also performs basic validation and
     * object construction (algorithms and dataset).
     *
     * @param argc  Argument count from main().
     * @param argv  Argument vector from main().
     * @return Fully populated ParsedParams structure.
     */
    ParsedParams parseCommandLine(int argc, char **argv);
}
