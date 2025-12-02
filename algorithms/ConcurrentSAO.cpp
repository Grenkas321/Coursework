#include "ConcurrentSAO.h"
#include "additionals.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <random>

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
        // Параметры, которые пойдут внутрь SimulatedAnnealing
        algo_params_ = {
            {"min_temp", min_temp},
            {"max_temp", max_temp},
            {"reduction_rule", (unsigned)reduction_rule},
            {"saturation", saturation},
            {"improvement", improvement},
            {"seed", seed} // важно: чтобы SAO внутри CSAO имел тот же seed
        };
    }

    // ---- Главная функция: сейчас просто обёртка над SimulatedAnnealing ----

    Schedule ConcurrentSAO::schedule_(const Graph &graph)
    {
        conveyor.clear();

        // Создаём наш обычный SAO
        SimulatedAnnealing sao;
        sao.setParams(algo_params_);

        // ВАЖНО: вызываем базовый BaseOptimization::schedule(graph),
        // а не sao.schedule(graph), которого нет.
        BaseOptimization &base = sao;
        Schedule best = base.schedule(graph);

        // Чисто для истории – сохраним конечную стоимость
        conveyor.push_back({best.cost()});

        return best;
    }

    // ---- Вспомогательные функции разбиения (на будущее, сейчас schedule_ их не использует) ----

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
     * Check if there is a path in the directed graph between two given nodes.
     */
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

    /**
     * Check if two nodes are adjacent in the graph.
     */
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

    // ---- Служебные методы ----

    std::unique_ptr<BaseOptimization> ConcurrentSAO::copy() const
    {
        return std::unique_ptr<BaseOptimization>(new ConcurrentSAO(*this));
    }

    ParamSet ConcurrentSAO::getParams() const
    {
        auto params = algo_params_; // сюда уже входит "seed"
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
                // label и прочие глобальные параметры BaseOptimization
                algo_params_[param] = val;
                BaseOptimization::setParams({{param, val}});
            }
        }
    }
}
