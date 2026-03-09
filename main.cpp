#include <iostream>
#include "parse.h"
#include "experiments.h"
#include "RandomSearch.h"
#include <fstream>
#include <ostream>
#include <iterator>
#include <exception>
#include <string>

namespace sp = scheduling_problem;
typedef sp::experiments::SaveParams::Mode SaveMode;

int main(int argc, char **argv)
{
    try
    {
        auto parsed_params = parse::parseCommandLine(argc, argv);
        if (parsed_params.help_requested)
            return 0;

        std::unordered_map<parse::Command, std::string> tmapper = {
            {parse::Command::SCHEDULE, "schedule"},
            {parse::Command::RUN, "run"},
            {parse::Command::STABILITY, "stability"}};
        std::cout << "Command: " << tmapper[parsed_params.command] << std::endl;
        std::cout << "Algos:\n";
        for (auto &alg : parsed_params.algorithms)
        {
            if (!alg)
                continue;
            auto params = alg->getParams();
            std::cout << "\t" << params["label"] << ":" << std::endl;
            for (auto &[param, val] : params)
            {
                if (param != "label")
                    std::cout << "\t  " << param << ": " << val << std::endl;
            }
            std::cout << std::endl;
        }
        std::cout << "Output path: " << parsed_params.output_path << std::endl;

        if (parsed_params.command != parse::Command::SCHEDULE)
            std::cout << "Duplicates: " << parsed_params.duplicates
                      << "\nNumber of threads: " << parsed_params.n_threads << std::endl;
        sp::experiments::SaveParams config(true, true, SaveMode::ONLYBEST, SaveMode::ONLYBEST);
        switch (parsed_params.command)
        {
        case parse::Command::SCHEDULE:
            for (auto &alg : parsed_params.algorithms)
            {
                if (!alg)
                    continue;
                sp::additionals::DAGPool::Batch batch;
                std::string algo_prefix_path = parsed_params.output_path + "/" + alg->label() + "_";
                while ((batch = parsed_params.data->nextBatch()))
                    for (auto &graph : batch)
                    {
                        auto schedule = alg->schedule(graph);
                        schedule.dump(algo_prefix_path + graph.name() + ".json");
                    }
            }
            break;
        case parse::Command::RUN:
            sp::experiments::runOnPool(parsed_params.algorithms,
                                       *parsed_params.data,
                                       parsed_params.output_path,
                                       parsed_params.duplicates,
                                       parsed_params.n_threads,
                                       config);
            break;
        case parse::Command::STABILITY:
            sp::experiments::stabilityByRunsOnPool(parsed_params.algorithms,
                                                   *parsed_params.data,
                                                   parsed_params.output_path,
                                                   parsed_params.dupset,
                                                   parsed_params.n_threads);
            break;
        }

        std::cout << "\a";
        return 0;
    }
    catch (const std::exception &e)
    {
        const std::string msg = e.what();
        std::cerr << "Failed to build schedule: " << msg << std::endl;
        if (msg.find("Memory limit") != std::string::npos ||
            msg.find("memory limit") != std::string::npos ||
            msg.find("No feasible greedy placement") != std::string::npos ||
            msg.find("feasible layered schedule") != std::string::npos)
        {
            std::cerr << "Невозможно построить расписание, удовлетворяющее заданному ограничению по памяти." << std::endl;
        }
        return 1;
    }
}
