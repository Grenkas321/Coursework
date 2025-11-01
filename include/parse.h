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
 * Methods for data input from command line
 */
namespace parse
{
    using namespace scheduling_problem;
    using namespace scheduling_problem::additionals;
    using namespace scheduling_problem::algorithms;
    namespace bpo = boost::program_options;

    /**
     * Vector of stored algorithms
     */
    typedef std::vector<std::unique_ptr<BaseOptimization>> Algorithms;
    /**
     * Map of algorithm names and their parameters
     */
    typedef std::unordered_map<std::string, std::unordered_map<std::string, std::vector<parameter>>> Grid;

    /**
     * Possible commands for the algorithm
     */
    enum class Command
    {
        /**
         * Run algorithm on given data
         */
        RUN,
        /**
         * Check stability, by running several times on same data
         */
        STABILITY,
        /**
         * Construct a %schedule for a single graph
         */
        SCHEDULE
    };

    /**
     * Stores all parameters that were parsed from the command line
     */
    struct ParsedParams
    {
        /**
         * Command to execute
         */
        Command command;
        /**
         * Which algorithm to use
         */
        Algorithms algorithms;
        /**
         * Input data
         */
        std::unique_ptr<DAGPool> data;
        /**
         * Path to output directory
         */
        std::string output_path;
        /**
         * Number of duplicates
         */
        unsigned duplicates;
        /**
         * Number of threads for parallel execution of experiments
         */
        unsigned n_threads;
        /**
         * Vector of duplicates, each element is the number of repetitive runs of algorithm on the same data
         */
        std::vector<unsigned> dupset;
        /**
         * Parameter grid
         */
        Grid grid;
    };

    /**
     * Parse command line arguments from the user input
     */
    ParsedParams parseCommandLine(int argc, char **argv);
}
