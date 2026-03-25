#include "parse.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include "ConcurrentSAO.h"

namespace parse
{
    /**
     * @brief Create optimizers by their short names.
     * @details Factory function that maps textual identifiers (e.g. "greedy", "aco", "sao", "csao", "sp")
     *          to corresponding optimizer instances with default parameters. JSON file names are handled
     *          in @ref readOptimizers instead.
     * @param algs List of optimizer identifiers or JSON file names.
     * @return Vector of polymorphic optimizers ready to be used in experiments.
     */
    Algorithms makeOptimizers(const std::vector<std::string> &algs);

    namespace
    {
        unsigned parseUnsigned(const nlohmann::json &value, const std::string &name)
        {
            if (!value.is_number_integer() && !value.is_number_unsigned())
                throw std::invalid_argument("Expected unsigned integer for '" + name + "'");
            const auto raw = value.get<long long>();
            if (raw < 0)
                throw std::invalid_argument("Expected non-negative value for '" + name + "'");
            return static_cast<unsigned>(raw);
        }
    }

    /**
     * @brief Convert a JSON scalar to a @c scheduling_problem::parameter.
     * @details Supports JSON string, unsigned integer, signed integer, and floating-point values.
     *          Non-scalar values result in a default-constructed parameter (object).
     * @param val JSON value to convert.
     * @return Parameter containing the converted value.
     */
    parameter fromJsonValue(const nlohmann::json &val)
    {
        scheduling_problem::parameter param;
        if (val.is_string())
            param = (std::string)val;
        else if (val.is_number_unsigned())
            param = (unsigned)val;
        else if (val.is_number_integer())
            param = (int)val;
        else if (val.is_number_float())
            param = (double)val;
        return param;
    }

    /**
     * @brief Build a set of optimizers from a JSON config file.
     * @details Reads a JSON object of the form:
     *          {
     *            "aco{…}": { "baseline": "greedy", "param1": 1, "param2": 0.5, … },
     *            "csao{…}": { "baseline": "greedy", "threads": 4, … },
     *            …
     *          }
     *          For "aco"/"sao" entries, constructs an iterative optimizer with the specified baseline
     *          and assigns parameters. For "csao", builds @c ConcurrentSAO directly and applies params.
     * @param alg_json Path to the JSON file describing optimizers and their parameters.
     * @return Vector of configured optimizers (baselines are embedded inside iterative ones).
     * @throws std::exception If the JSON file cannot be parsed.
     */
    Algorithms readOptimizers(const std::string &alg_json)
    {
        Algorithms algorithms;
        auto json = nlohmann::json::parse(std::ifstream(alg_json));
        for (auto &alg : json.items())
        {
            auto baseline = BASELINE.copy();
            // you can read only iterative optimizers (optimizers that have params!!!)
            std::unique_ptr<IterativeOptimization> optimizer;
            std::unordered_map<std::string, parameter> params;
            auto data = alg.value();
            for (auto &param : data.items())
            {
                std::string key(param.key().c_str());
                auto val = data[key];

                if (key == "baseline")
                {
                    if (!val.is_string())
                        throw std::invalid_argument("Field 'baseline' must be a string in " + alg_json);
                    auto baseline_opt = makeOptimizers({val.get<std::string>()});
                    if (baseline_opt.empty() || !baseline_opt[0])
                        throw std::invalid_argument("Unknown baseline algorithm: " + val.get<std::string>());
                    baseline = std::move(baseline_opt[0]);
                }
                else
                    params[key] = fromJsonValue(val);
            }

            std::string alg_label(alg.key());
            if (alg_label.substr(0, 3) == "aco")
                optimizer = std::unique_ptr<AntColonySystem>(new AntColonySystem(*baseline));
            else if (alg_label.substr(0, 3) == "sao")
                optimizer = std::unique_ptr<SimulatedAnnealing>(new SimulatedAnnealing(*baseline));
            else if (alg_label.substr(0, 4) == "csao")
            {
                auto algo = std::unique_ptr<ConcurrentSAO>(new ConcurrentSAO());
                algo->setParams(params);
                algorithms.push_back(std::move(algo));
                continue;
            }
            else
            {
                throw std::invalid_argument("Unsupported optimizer label in JSON: " + alg_label);
            }

            if (!optimizer)
                throw std::invalid_argument("Failed to build optimizer for label: " + alg_label);
            optimizer->setParams(params);

            algorithms.push_back(std::move(optimizer));
        }
        return algorithms;
    }

    /**
     * @brief Create optimizers by names or include nested JSON configurations.
     * @details For items ending with ".json" this function delegates to @ref readOptimizers and
     *          appends the resulting optimizers. For plain names it constructs a single optimizer
     *          with default parameters.
     * @param algs List of algorithm tokens (names or paths to JSON files).
     * @return Vector of optimizers in the order specified by @p algs.
     */
    Algorithms makeOptimizers(const std::vector<std::string> &algs)
    {
        Algorithms algorithms;
        std::string optimizer_name;
        for (const auto &alg : algs)
        {
            if (alg.size() > 5 && alg.substr(alg.size() - 5, 5) == ".json")
            {
                auto sub_algs = readOptimizers(alg);
                for (auto &sub_alg : sub_algs)
                    algorithms.push_back(std::move(sub_alg));
            }
            else
            {
                std::unique_ptr<BaseOptimization> optimizer;
                if (alg == "aco")
                    optimizer = std::unique_ptr<AntColonySystem>(new AntColonySystem());
                else if (alg == "sao")
                    optimizer = std::unique_ptr<SimulatedAnnealing>(new SimulatedAnnealing());
                else if (alg == "csao")
                    optimizer = std::unique_ptr<ConcurrentSAO>(new ConcurrentSAO());
                else if (alg == "greedy")
                    optimizer = std::unique_ptr<Greedy>(new Greedy());
                else if (alg == "sp")
                    optimizer = std::unique_ptr<SeriesParallel>(new SeriesParallel("sp"));
                else if (alg == "sp0")
                    optimizer = std::unique_ptr<SeriesParallel>(new SeriesParallel("sp0"));

                if (!optimizer)
                    throw std::invalid_argument("Unsupported algorithm: " + alg);
                algorithms.push_back(std::move(optimizer));
            }
        }
        return algorithms;
    }

    /**
     * @brief Read generator parameters for synthetic DAGs.
     * @details Expected JSON structure:
     *          {
     *            "vertex_edge_map": { "V1": E1, "V2": E2, ... },
     *            "weights": [vertex_weight_minmax, edge_weight_minmax],
     *            "prefix": "dag",
     *            "seed": [seed_value]
     *          }
     * @param path Path to the JSON file with generator configuration.
     * @return Tuple of (vertex/edge counts grid, min/max weights pair, file prefix, PRNG seed).
     * @throws std::exception If the JSON file cannot be parsed.
     */
    std::tuple<std::vector<std::pair<weight_t, weight_t>>,
               std::pair<weight_t, weight_t>, std::string, unsigned>
    readGraphParams(const std::string &path)
    {
        std::vector<std::pair<weight_t, weight_t>> vertedge_map;
        std::pair<scheduling_problem::weight_t, scheduling_problem::weight_t> weights{1, 1};
        std::string prefix("dag");
        unsigned seed(42);
        auto json = nlohmann::json::parse(std::ifstream(path));
        for (auto &item : json.items())
        {
            auto values = item.value();
            if (item.key() == "vertex_edge_map")
            {
                for (auto &val : values.items())
                {
                    const auto n_vertices = static_cast<unsigned>(std::stoull(val.key()));
                    const auto n_edges = parseUnsigned(val.value(), "vertex_edge_map");
                    vertedge_map.push_back({n_vertices, n_edges});
                }
            }
            else if (item.key() == "prefix")
                prefix = (std::string)fromJsonValue(values);
            else if (item.key() == "weights")
            {
                if (!values.is_array() || values.size() < 2)
                    throw std::invalid_argument("'weights' must be an array with 2 elements");
                weights = std::make_pair(static_cast<weight_t>(values[0].get<long long>()),
                                         static_cast<weight_t>(values[1].get<long long>()));
            }
            else if (item.key() == "seed")
            {
                if (!values.is_array() || values.empty())
                    throw std::invalid_argument("'seed' must be a non-empty array");
                seed = parseUnsigned(values[0], "seed");
            }
        }
        return std::make_tuple(vertedge_map, weights, prefix, seed);
    }

    /**
     * @brief Read a grid of algorithm parameters from JSON.
     * @details The file is expected to contain a mapping from algorithm label to a map of
     *          parameter name → list of values. Each scalar is converted with @ref fromJsonValue.
     * @param path Path to the JSON file with parameter grid.
     * @return Grid structure suitable for sweeping experiments.
     * @throws std::exception If the JSON file cannot be parsed.
     */
    Grid readParamGrid(const std::string &path)
    {
        Grid param_grids;
        auto json = nlohmann::json::parse(std::ifstream(path));
        for (auto item : json.items())
        {
            param_grids[item.key()] = std::unordered_map<std::string, std::vector<scheduling_problem::parameter>>();
            for (auto params : item.value().items())
            {
                param_grids[item.key()][params.key()] = std::vector<scheduling_problem::parameter>();
                for (auto val : params.value())
                {
                    param_grids[item.key()][params.key()].push_back(fromJsonValue(val));
                }
            }
        }
        return param_grids;
    }

    /**
     * @brief Build a data pool from either a directory of DAGs or a generator spec.
     * @details If @p path ends with ".json", constructs a @c DAGGenerator using parameters
     *          returned by @ref readGraphParams. Otherwise, loads DAGs via @c DAGReader.
     * @param path Path to a directory with DAG files or a JSON generator spec.
     * @param n_samples Number of graphs to include (0 means use all in directory).
     * @param batch_size Number of graphs per batch during experiments.
     * @return Unique pointer to a polymorphic @c DAGPool.
     * @throws std::exception If the generator JSON cannot be parsed.
     */
    std::unique_ptr<DAGPool> makeDataPool(const std::string &path,
                                          unsigned n_samples,
                                          unsigned batch_size)
    {
        DAGPool *data;
        if (path.size() >= 5 && path.substr(path.size() - 5, 5) == ".json")
        {
            auto params = readGraphParams(path);
            auto vertedge_map = std::get<0>(params);
            auto weights = std::get<1>(params);
            auto prefix = std::get<2>(params);
            auto seed = std::get<3>(params);
            data = new DAGGenerator(vertedge_map, weights, n_samples, batch_size, seed, prefix);
        }
        else
        {
            data = new DAGReader(path, n_samples, batch_size);
        }
        return std::unique_ptr<DAGPool>(data);
    }

    /**
     * @brief Parse command-line arguments and assemble experiment parameters.
     * @details Two-phase parsing is used. First pass recognizes @c --help and prints the
     *          available options. The second pass reads general options and additional options
     *          for the selected command. Supported commands:
     *          - @c schedule  (default)
     *          - @c run       (adds @c --dups)
     *          - @c stability (adds @c --dups with a list)
     * @param argc Argument count from @c main.
     * @param argv Argument vector from @c main.
     * @return Filled @c ParsedParams structure. If @c --help was requested, returns defaults.
     * @throws boost::program_options::required_option If required options are missing.
     */
    ParsedParams parseCommandLine(int argc, char **argv)
    {
        /* Initialize params descriptions */
        ParsedParams params;
        std::string command;
        bpo::options_description help_desc("You may use following options to config each experiment");
        bpo::options_description general_desc("");
        help_desc.add_options()("help,h", "Show this description")("command,c", bpo::value<std::string>(&command)->default_value("schedule"),
                                                                   "Specify experiment type (schedule, run, stability); default is 'schedule'");
        general_desc.add_options()("output,o", bpo::value<std::string>(&params.output_path)->required(),
                                   "Specify output directory") //
            ("input,i", bpo::value<std::string>()->required(),
             "Specify input data directory") //
            ("samples,s", bpo::value<unsigned>()->default_value(0),
             "Specify number of graphs for experiment; by default, all DAG files in the input directory are used") //
            ("batch,b", bpo::value<unsigned>()->default_value(5),
             "Specify batch size, each batch results are recorded to separate table; default is 5") //
            ("algo,a", bpo::value<std::vector<std::string>>()->required(),
             "Specify algorithms for experiment; you may use algorithms with default parameters "
             "(specify 'aco', 'sao', 'base', 'greedy') or specify algorithm and parameters in the json file"
             "(see examples of json files in /data)") //
            ("processors,p", bpo::value<unsigned>(&params.processors)->default_value(1),
             "Specify number of processors in multiprocessor model; default is 1") //
            ("memory,m", bpo::value<weight_t>(&params.memory_limit)->default_value(std::numeric_limits<weight_t>::max()),
             "Specify hard memory limit M in model units; default is unlimited") //
            ("threads,t", bpo::value<unsigned>()->default_value(1),
             "Specify number of threads for experiment; every algorithm-batch pair will be "
             "executed in a separate thread; default is 1");
        bpo::options_description run_desc("For 'run' command you may specify number of runs");
        run_desc.add_options()("dups,d", bpo::value<unsigned>()->default_value(10),
                               "Specify number of runs on every batch");
        bpo::options_description
            stability_desc("For 'stability' command you have to specify set of number of runs");
        stability_desc.add_options()("dups,d", bpo::value<std::vector<unsigned>>()->required(),
                                     "Specify set of number of runs on every batch");

        /* Parse and check command line */
        bpo::command_line_parser parser(argc, argv);
        bpo::parsed_options parsed = parser.options(help_desc).allow_unregistered().run();
        bpo::variables_map vm;
        bpo::store(parsed, vm);
        bpo::notify(vm);
        if (vm.count("help"))
        {
            std::cout << help_desc << std::endl
                      << general_desc << std::endl
                      << run_desc
                      << std::endl
                      << stability_desc << std::endl;
            params.help_requested = true;
            return params;
        }
        else
        {
            general_desc.add(help_desc);
            if (command == "schedule")
            {
                params.command = Command::SCHEDULE;
            }
            else if (command == "run")
            {
                params.command = Command::RUN;
                general_desc.add(run_desc);
            }
            else if (command == "stability")
            {
                params.command = Command::STABILITY;
                general_desc.add(stability_desc);
            }
            else
            {
                throw std::invalid_argument("Unknown command: " + command);
            }
        }

        parsed = bpo::command_line_parser(argc, argv).options(general_desc).run();
        bpo::store(parsed, vm);
        bpo::notify(vm);

        /* Make structures for experiments */
        params.algorithms = makeOptimizers(vm["algo"].as<std::vector<std::string>>());
        if (params.algorithms.empty())
            throw std::invalid_argument("No algorithms selected");
        scheduling_problem::algorithms::ParamSet shared_params{
            {"processors", params.processors},
            {"memory_limit", static_cast<double>(params.memory_limit)}
        };
        for (auto &alg : params.algorithms)
            if (alg)
                alg->setParams(shared_params);
        params.data = makeDataPool(vm["input"].as<std::string>(),
                                   vm["samples"].as<unsigned>(),
                                   vm["batch"].as<unsigned>());
        params.n_threads = std::max(1u, vm["threads"].as<unsigned>());
        switch (params.command)
        {
        case Command::RUN:
            params.duplicates = vm["dups"].as<unsigned>();
            break;
        case Command::STABILITY:
            params.dupset = vm["dups"].as<std::vector<unsigned>>();
            break;
        case Command::SCHEDULE:
            break;
        }

        return params;
    }
}
