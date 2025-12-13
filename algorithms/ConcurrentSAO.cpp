#include "ConcurrentSAO.h"
#include "additionals.h"
#include "Greedy.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <random>
#include <future>
#include <chrono>
#include <cstddef>

namespace {

    // Всегда получаем новый seed для каждого запуска (и для каждой волны SAO внутри CSAO).
    static unsigned runtime_seed(unsigned salt = 0u)
    {
        std::random_device rd;
        const unsigned t = (unsigned)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return rd() ^ (t + 0x9e3779b9u + (salt << 6) + (salt >> 2));
    }

}

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
        : BaseOptimization(label),
          partitions_count_(partitions_count),
          rsearch_iters_(rsearch_iters),
          warm_start_(warm_start),
          subareas_(subareas),
          rng_(seed)
    {
        algo_params_ = {
            {"min_temp", min_temp},
            {"max_temp", max_temp},
            {"reduction_rule", (unsigned)reduction_rule},
            {"saturation", saturation},
            {"improvement", improvement},
            {"seed", seed}
        };
    }

    Schedule ConcurrentSAO::schedule_(const Graph &graph)
    {
        conveyor.clear();

        // RNG CSAO: каждый запуск CSAO должен быть стохастическим.
        const unsigned csao_seed = runtime_seed();
        rng_.seed(csao_seed);
        algo_params_["seed"] = csao_seed;

        // 1) Стартовое решение: Greedy
        Greedy greedy;
        Schedule bestOverall = greedy.schedule(graph);

        unsigned noImprove = 0;
        const unsigned saturation = (unsigned)algo_params_["saturation"];

        // Снимем параметры SAO в локальные значения (чтобы не читать map из потоков).
        const double min_temp = (double)algo_params_["min_temp"];
        const double max_temp = (double)algo_params_["max_temp"];
        const ReductionRules rule = (ReductionRules)(unsigned)algo_params_["reduction_rule"];
        const unsigned sao_saturation = (unsigned)algo_params_["saturation"];
        const double improvement = (double)algo_params_["improvement"];

        // 2) Конвейер: волны из 6 параллельных SAO, каждый стартует с лучшего прошлого результата.
        while (noImprove < saturation)
        {
            const Schedule wave_base = bestOverall; // фиксируем старт волны

            std::vector<std::future<Schedule>> futs;
            futs.reserve(6);

            for (unsigned i = 0; i < 6; ++i)
            {
                // у каждого SAO свой seed (дополнительно солим номером i)
                const unsigned local_seed = runtime_seed(i + 1);

                futs.emplace_back(std::async(std::launch::async,
                                             [&, local_seed, wave_base, min_temp, max_temp, rule, sao_saturation, improvement]() -> Schedule
                {
                    SimulatedAnnealing sao(
                        BASELINE,
                        min_temp,
                        max_temp,
                        rule,
                        sao_saturation,
                        improvement,
                        local_seed,
                        "csao_sao");

                    return sao.schedule(graph, wave_base);
                }));
            }

            std::vector<Schedule> results;
            results.reserve(6);
            for (auto &f : futs)
                results.push_back(f.get());

            auto bestIt = std::min_element(
                results.begin(), results.end(),
                [](const Schedule &a, const Schedule &b)
                { return a.cost() < b.cost(); });

            if (bestIt != results.end() && bestIt->cost() < bestOverall.cost())
            {
                bestOverall = *bestIt;
                noImprove = 0;
            }
            else
            {
                ++noImprove;
            }
        }

        return bestOverall;
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

    bool isDirectedPathExist(size_t source,
                             size_t target,
                             const ConcurrentSAO::AdjacencyMatrix &adjacency_matrix,
                             std::unordered_set<size_t> &visited)
    {
        bool is_exist = false;
        for (size_t child = 0; child < adjacency_matrix.size(); ++child)
        {
            if (visited.find(child) == visited.end() && adjacency_matrix[source][child])
            {
                visited.insert(child);
                is_exist = is_exist ||
                           (child == target ||
                            isDirectedPathExist(child, target, adjacency_matrix, visited));
            }
        }
        return is_exist;
    }

    bool isRelated(size_t source,
                   size_t target,
                   const ConcurrentSAO::AdjacencyMatrix &adjacency_matrix)
    {
        std::unordered_set<size_t> visited1, visited2;
        return isDirectedPathExist(source, target, adjacency_matrix, visited1) ||
               isDirectedPathExist(target, source, adjacency_matrix, visited2);
    }

    std::pair<size_t, size_t> ConcurrentSAO::subarea(const AdjacencyMatrix &adjacency_matrix)
    {
        std::uniform_int_distribution<size_t> uid(0, adjacency_matrix.size() - 1);
        bool stop = false;
        size_t row = 0, col = 0;
        while (!stop)
        {
            row = uid(rng_);
            std::vector<size_t> unrelated;
            for (size_t c = 0; c < adjacency_matrix.size(); ++c)
            {
                if (row != c && !isRelated(row, c, adjacency_matrix))
                    unrelated.push_back(c);
            }
            if (!unrelated.empty())
            {
                size_t index =
                    std::uniform_int_distribution<size_t>(0, unrelated.size() - 1)(rng_);
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
        for (unsigned partition = 0; partition < partitions_count_; ++partition)
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
            data = std::move(subdata);
        }
        for (unsigned id = 0; id < data.size(); ++id)
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
            else if (param == "seed")
            {
                auto s = (unsigned)val;
                algo_params_["seed"] = s;
                rng_.seed(s);
            }
            else
            {
                algo_params_[param] = val;
                BaseOptimization::setParams({{param, val}});
            }
        }
    }
}
