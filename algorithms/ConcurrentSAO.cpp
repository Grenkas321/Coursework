#include "ConcurrentSAO.h"
#include "ScheduleChecker.h"
#include "RandomSearch.h"
#include "additionals.h"

namespace scheduling_problem::algorithms
{
    ConcurrentSAO::ConcurrentSAO(unsigned partitions_count,
                                 unsigned rsearch_iters,
                                 bool warm_start,
                                 bool subareas,
                                 double min_temp,
                                 double max_temp,
                                 ReductionRules reduction_rule,
                                 unsigned saturation,
                                 double improvement,
                                 unsigned seed,
                                 const std::string &label)
        : BaseOptimization(label), partitions_count_(partitions_count), rsearch_iters_(rsearch_iters), warm_start_(warm_start), subareas_(subareas), rng_(seed)
    {
        algo_params_ = {
            {"min_temp", min_temp},
            {"max_temp", max_temp},
            {"reduction_rule", (unsigned)reduction_rule},
            {"saturation", saturation},
            {"improvement", improvement}};
    }

    Schedule ConcurrentSAO::schedule_(const Graph &graph)
    {
        conveyor.clear();
        Schedule best_schedule;
        std::vector<Graph> data;
        if (subareas_)
        {
            data = makePartitions(graph);
        }
        else
        {
            for (unsigned count(0); count < std::pow(2, partitions_count_); count++)
            {
                data.push_back(graph);
                data[count].rename(std::to_string(count));
            }
        }
        std::uniform_int_distribution<unsigned> uid(0, ((size_t)1 << 31) - 1);
        std::vector<Schedule> baselines;
        RandomSearch rsearch(rsearch_iters_);
        Greedy greedy;
        additionals::ScheduleChecker checker(true);
        for (auto &sample : data)
        {
            auto baseline = greedy.schedule(sample);
            if (!best_schedule.size() || baseline.cost() < best_schedule.cost())
                best_schedule = baseline;
            baselines.push_back(std::move(baseline));
            std::cout << graph.name() << " Baseline: " << baseline.cost() << std::endl;
        }
        while (data.size())
        {
            std::vector<std::future<Schedule>> schedules;
            std::vector<weight_t> costs;
            for (unsigned id(0); id < data.size(); id++)
            {
                auto seed = uid(rng_);
                schedules.push_back(
                    std::async(
                        std::launch::async, [](ParamSet algo_params, unsigned seed, Graph sample, Schedule baseline)
                        {
                        SimulatedAnnealing sao;
                        algo_params["seed"] = seed;
                        sao.setParams(algo_params);
                        return sao.schedule(sample, baseline); },
                        algo_params_, seed, data[id], baselines[id]));
            }

            for (unsigned id(0); id < schedules.size(); id++)
            {
                baselines[id] = schedules[id].get();
                costs.push_back(baselines[id].cost());
            }

            std::sort(baselines.begin(), baselines.end(),
                      [](const auto &schd1, const auto &schd2)
                      { return schd1.cost() < schd2.cost(); });
            if (baselines[0].cost() < best_schedule.cost())
                best_schedule = baselines[0];
            baselines.erase(baselines.begin() + baselines.size() / 2, baselines.end());
            std::vector<Graph> bufdata;
            for (auto &baseline : baselines)
            {
                for (auto &sample : data)
                {
                    if (sample.name() == baseline.name())
                        bufdata.push_back(std::move(sample));
                }
            }
            data = bufdata;
            conveyor.push_back(costs);
        }
        return best_schedule;
    }

    ConcurrentSAO::AdjacencyMatrix ConcurrentSAO::makeAdjacencyMatrix(const Graph &graph)
    {
        size_t nb_vertex = boost::num_vertices(graph);
        AdjacencyMatrix adjacency_matrix(nb_vertex, std::vector<bool>(nb_vertex, false));
        for (const auto &vertex : boost::make_iterator_range(boost::vertices(graph)))
        {
            for (const auto &child : boost::make_iterator_range(boost::adjacent_vertices(vertex, graph)))
                adjacency_matrix[vertex][child] = true;
        }
        return adjacency_matrix;
    }

    /**
     * Check if there is a path in the directed graph between two given nodes
     */
    bool isDirectedPathExist(size_t source, size_t target,
                             const ConcurrentSAO::AdjacencyMatrix &adjacency_matrix, std::unordered_set<size_t> &visited)
    {
        bool is_exist(false);
        for (size_t child(0); child < adjacency_matrix.size(); child++)
        {
            if (visited.find(child) == visited.end() && adjacency_matrix[source][child])
            {
                visited.insert(child);
                is_exist |= (child == target || isDirectedPathExist(child, target, adjacency_matrix, visited));
            }
        }
        return is_exist;
    }

    /**
     * Check if two nodes are adjacent in the graph
     */
    bool isRelated(size_t source, size_t target,
                   const ConcurrentSAO::AdjacencyMatrix &adjacency_matrix)
    {
        std::unordered_set<size_t> visited1, visited2;
        return isDirectedPathExist(source, target, adjacency_matrix, visited1) ||
               isDirectedPathExist(target, source, adjacency_matrix, visited2);
    }

    std::pair<size_t, size_t> ConcurrentSAO::subarea(const AdjacencyMatrix &adjacency_matrix)
    {
        std::uniform_int_distribution<size_t> uid(0, adjacency_matrix.size() - 1);
        bool stop(false);
        size_t row, col;
        while (!stop)
        {
            row = uid(rng_);
            std::vector<size_t> unrelated;
            for (size_t col(0); col < adjacency_matrix.size(); col++)
            {
                if (row != col && !isRelated(row, col, adjacency_matrix))
                    unrelated.push_back(col);
            }
            if (unrelated.size())
            {
                size_t index = std::uniform_int_distribution<size_t>(0, unrelated.size() - 1)(rng_);
                col = unrelated[index];
                stop = true;
            }
        }
        return {row, col};
    }

    std::vector<Graph> ConcurrentSAO::makePartitions(const Graph &graph)
    {
        std::vector<Graph> data{graph};
        auto adjacency_matrix = makeAdjacencyMatrix(graph);
        for (unsigned partition(0); partition < partitions_count_; partition++)
        {
            std::vector<Graph> subdata;
            auto [v1, v2] = subarea(adjacency_matrix);
            for (auto &sample : data)
            {
                subdata.push_back(sample);
                subdata.back().addImEdge(v1, v2);
                subdata.push_back(sample);
                subdata.back().addImEdge(v2, v1);
                adjacency_matrix[v1][v2] = adjacency_matrix[v2][v1] = true;
            }
            data = subdata;
        }
        for (unsigned id(0); id < data.size(); id++)
        {
            data[id] = additionals::topologicalSort(data[id]).first;
            data[id].rename(std::to_string(id));
        }
        return data;
    }

    std::unique_ptr<BaseOptimization> ConcurrentSAO::copy() const
    {
        return std::unique_ptr<BaseOptimization>(new ConcurrentSAO(*this));
    }

    ParamSet ConcurrentSAO::getParams() const
    {
        auto params = algo_params_;
        params["rsearch_iters"] = rsearch_iters_;
        params["partitions_count"] = partitions_count_;
        params["warm_start"] = warm_start_;
        params["subareas"] = subareas_;
        params["label"] = label();
        return params;
    }

    void ConcurrentSAO::setParams(const ParamSet &params)
    {
        for (auto &[param, val] : params)
        {
            if (param == "partitions_count")
                partitions_count_ = (unsigned)val;
            else if (param == "rsearch_iters")
                rsearch_iters_ = (unsigned)val;
            else if (param == "warm_start")
                warm_start_ = (bool)val;
            else if (param == "subareas")
                subareas_ = (bool)val;
            else if (param == "min_temp")
                algo_params_["min_temp"] = (double)val;
            else if (param == "max_temp")
                algo_params_["max_temp"] = (double)val;
            else if (param == "reduction_rule")
                algo_params_["reduction_rule"] = (unsigned)val;
            else if (param == "saturation")
                algo_params_["saturation"] = (unsigned)val;
            else if (param == "improvement")
                algo_params_["improvement"] = (double)val;
            else
                BaseOptimization::setParams({{param, val}});
        }
    }
}
