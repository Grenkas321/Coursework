#include "parse.h"
#include <fstream>
#include "ConcurrentSAO.h"

namespace parse
{
    /**
     * Creates number of solvers according to the given list of names
     */
    Algorithms makeOptimizers(const std::vector<std::string> &algs);

    /**
     * Parses parameters from the JSON structure
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
     * Creates solvers from the given string of names
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
                    baseline = std::move(makeOptimizers({val})[0]);
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

            optimizer->setParams(params);

            algorithms.push_back(std::move(optimizer));
        }
        return algorithms;
    }

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
                else if (alg == "greedy")
                    optimizer = std::unique_ptr<Greedy>(new Greedy());
                else if (alg == "sp")
                    optimizer = std::unique_ptr<SeriesParallel>(new SeriesParallel());

                algorithms.push_back(std::move(optimizer));
            }
        }
        return algorithms;
    }

    /**
     * Read parameters of the generated graphs from the given file
     */
    std::tuple<std::vector<std::pair<weight_t, weight_t>>,
               std::pair<weight_t, weight_t>, std::string, unsigned>
    readGraphParams(const std::string &path)
    {
        std::vector<std::pair<weight_t, weight_t>> vertedge_map;
        std::pair<scheduling_problem::weight_t, scheduling_problem::weight_t> weights;
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
                    unsigned n_vertices = fromJsonValue(val.key());
                    unsigned n_edges = fromJsonValue(val.value());
                    vertedge_map.push_back({n_vertices, n_edges});
                }
            }
            else if (item.key() == "prefix")
                prefix = (std::string)fromJsonValue(values);
            else if (item.key() == "weights")
                weights = std::make_pair((weight_t)values[0], (weight_t)values[1]);
            else if (item.key() == "seed")
                seed = fromJsonValue(values[0]);
        }
        return std::make_tuple(vertedge_map, weights, prefix, seed);
    }

    /**
     * Read grid of parameters from the given file according to the path
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
     * Read graphs from the given direcotry and create DAGPool from them
     */
    std::unique_ptr<DAGPool> makeDataPool(const std::string &path,
                                          unsigned n_samples,
                                          unsigned batch_size)
    {
        DAGPool *data;
        if (path.substr(path.size() - 5, 5) == ".json")
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
        }

        parsed = bpo::command_line_parser(argc, argv).options(general_desc).run();
        bpo::store(parsed, vm);
        bpo::notify(vm);

        /* Make structures for experiments */
        params.algorithms = makeOptimizers(vm["algo"].as<std::vector<std::string>>());
        params.data = makeDataPool(vm["input"].as<std::string>(),
                                   vm["samples"].as<unsigned>(),
                                   vm["batch"].as<unsigned>());
        params.n_threads = vm["threads"].as<unsigned>();
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
