#include "AntColonySystem.h"
#include "RandomSearch.h"

/**
 * Scheduling algorithm classes
 */
namespace scheduling_problem::algorithms
{

    ScheduleStatus AntColonySystem::ArtificialAnt::makeRoute(const Graph &graph,
                                                             Matrix &matrix,
                                                             double phe_influence,
                                                             double heu_influence,
                                                             double threshold,
                                                             randgen &rng)
    {
        auto num_vertices = boost::num_vertices(graph);
        ScheduleStatus status(graph);
        status.insert(0, 0, graph);
        for (size_t curr_vid(1); curr_vid < num_vertices; curr_vid++)
        {
            auto pos = choice(graph, matrix, status, curr_vid, phe_influence, heu_influence, threshold, rng);
            status.insert(curr_vid, pos, graph);
        }

        return status;
    }

    size_t AntColonySystem::ArtificialAnt::choice(const Graph &graph,
                                                  const Matrix &matrix,
                                                  const ScheduleStatus &status,
                                                  size_t curr_vid,
                                                  double phe_influence,
                                                  double heu_influence,
                                                  double threshold,
                                                  randgen &rng)
    {
        auto desirability = heuInfo(graph, status, curr_vid);
        for (auto &[pos, heu_val] : desirability)
        {
            double phe_val{1.};
            for (size_t prev_vid(0); prev_vid < curr_vid; prev_vid++)
                if (status.loc(prev_vid) < pos)
                    phe_val *= matrix[prev_vid][curr_vid];
                else
                    phe_val *= matrix[curr_vid][prev_vid];
            heu_val = std::pow(phe_val, phe_influence) * std::pow(heu_val, heu_influence);
        }

        double q = std::uniform_real_distribution<double>(0, 1.)(rng);
        size_t pos = desirability.begin()->first;
        if (q > threshold)
        {
            double probs_sum = std::accumulate(desirability.begin(), desirability.end(), 0.0,
                                               [](const auto &init, const auto &elem)
                                               { return init + elem.second; });
            double cumsum = 0.0;
            double uniform_sample = std::uniform_real_distribution<double>(0, probs_sum)(rng);
            for (auto &prob : desirability)
            {
                cumsum += prob.second;
                if (cumsum > uniform_sample)
                    break;
                pos = prob.first;
            }
        }
        else
        {
            pos = std::max_element(desirability.begin(), desirability.end(),
                                   [](auto &prob_1, auto &prob_2)
                                   { return prob_1.second < prob_2.second; })
                      ->first;
        }

        return pos;
    }

    AntColonySystem::AntColonySystem(BaseOptimization &baseline,
                                     double evaporation,
                                     double phe_decay,
                                     double phe_influence,
                                     double heu_influence,
                                     double threshold,
                                     unsigned epochs_count,
                                     unsigned ants_count,
                                     unsigned best_count,
                                     unsigned saturation,
                                     double improvement,
                                     unsigned seed,
                                     const std::string &label)
        : IterativeOptimization(baseline,
                                saturation,
                                improvement,
                                seed,
                                label),
          evaporation_(evaporation), phe_decay_(phe_decay), phe_influence_(phe_influence), heu_influence_(heu_influence), threshold_(threshold), epochs_count_(epochs_count), ants_count_(ants_count), best_count_(best_count), rebuilder_(seed)
    {
        for (unsigned mul(0); mul < ants_count_; mul++)
            ants_.push_back(ArtificialAnt());
    }

    Schedule AntColonySystem::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        weight_t weight_max = 0;
        for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
            if (boost::get(vertex_weight_t(), graph, vertex) > weight_max)
                weight_max = boost::get(vertex_weight_t(), graph, vertex);

        auto n_vertices = boost::num_vertices(graph);
        auto best_solution = base_schedule;
        double init_trail = (double)weight_max / best_solution.cost();
        auto weight_matrix = makeMatrix(n_vertices, init_trail);
        unsigned stagnations = 0;
        iters_count_ = 0;

        ScheduleStatus status(graph, best_solution);
        std::vector<ScheduleStatus> solution_pool{status};
        RandomSearch rsearch(3000);

        for (unsigned i(1); i < best_count_ * 2; i++)
        {
            auto schedule = rsearch.generateRandomSchedule(graph);
            solution_pool.push_back(ScheduleStatus(graph, schedule));
        }

        std::sort(solution_pool.begin(), solution_pool.end(),
                  [](auto &route_1, auto &route_2)
                  { return route_1.cost() < route_2.cost(); });
        solution_pool.erase(solution_pool.begin() + best_count_, solution_pool.end());
        best_solution = solution_pool[0];

        for (unsigned epoch(0); epoch < epochs_count_; epoch++)
        {

            globalPheUpdate(solution_pool, weight_matrix, weight_max);

            auto epoch_routes = completeEpoch(graph, weight_matrix, init_trail); // sorted solutions

            if (epoch_routes[0].cost() < best_solution.cost())
            {
                auto improvement = (double)(best_solution.cost() - epoch_routes[0].cost()) / best_solution.cost();
                if (improvement > improvement_)
                    stagnations = 0;
                best_solution = epoch_routes[0];
            }

            solution_pool.insert(solution_pool.end(), epoch_routes.begin(), epoch_routes.end());
            std::sort(solution_pool.begin(), solution_pool.end(),
                      [](auto &route_1, auto &route_2)
                      { return route_1.cost() < route_2.cost(); });
            solution_pool.erase(solution_pool.begin() + best_count_, solution_pool.end());

            iters_count_++;
            stagnations++;
            cost_dynamics_.push_back(epoch_routes[0].cost());
            if (saturation_ && stagnations >= saturation_)
                break;
        }

        return best_solution;
    }

    std::vector<std::vector<double>> AntColonySystem::makeMatrix(size_t n_vertices, double pad) const
    {
        std::vector<std::vector<double>> matrix(n_vertices, std::vector<double>(n_vertices, pad));
        return matrix;
    }

    std::vector<ScheduleStatus> AntColonySystem::completeEpoch(const Graph &graph,
                                                               Matrix &matrix,
                                                               double init_trail)
    {
        std::vector<ScheduleStatus> routes;
        for (auto ant : ants_)
        {
            auto route = ant.makeRoute(graph,
                                       matrix,
                                       phe_influence_,
                                       heu_influence_,
                                       threshold_,
                                       rng_);
            routes.push_back(route);
            localPheUpdate(route, matrix, init_trail);
        }

        std::sort(routes.begin(), routes.end(),
                  [](auto &route_1, auto &route_2)
                  { return route_1.cost() < route_2.cost(); });

        return routes;
    }

    void AntColonySystem::localPheUpdate(const ScheduleStatus &status, Matrix &matrix, double delta) const
    {
        auto reinforcement = phe_decay_ * delta;
        auto alpha(1 - phe_decay_);
        for (size_t curr_vid(1); curr_vid < status.size(); curr_vid++)
            for (size_t prev_vid(0); prev_vid < curr_vid; prev_vid++)
                if (status.loc(prev_vid) < status.loc(curr_vid))
                    matrix[prev_vid][curr_vid] = alpha * matrix[prev_vid][curr_vid] + reinforcement;
                else
                    matrix[curr_vid][prev_vid] = alpha * matrix[curr_vid][prev_vid] + reinforcement;
    }

    void AntColonySystem::globalPheUpdate(const std::vector<ScheduleStatus> &epoch_routes,
                                          Matrix &matrix,
                                          double weight_max) const
    {
        for (auto &row : matrix)
            for (auto &elem : row)
                elem = (1 - evaporation_) * elem;

        for (auto &route : epoch_routes)
        {
            auto reinforcement = evaporation_ * (double)weight_max / route.cost();

            for (size_t u(0); u < route.size() - 1; u++)
            {
                for (size_t v(u + 1); v < route.size(); v++)
                {
                    if (route.loc(u) < route.loc(v))
                        matrix[u][v] += reinforcement;
                    else
                        matrix[v][u] += reinforcement;
                }
            }
        }
    }

    std::unique_ptr<BaseOptimization> AntColonySystem::copy() const
    {
        auto aco = std::unique_ptr<AntColonySystem>(new AntColonySystem());
        aco->setParams(this->getParams());
        return aco;
    }

    void AntColonySystem::setParams(const ParamSet &params)
    {
        for (auto &[param, val] : params)
        {
            if (param == "evaporation")
                evaporation_ = (double)val;
            else if (param == "phe_decay")
                phe_decay_ = (double)val;
            else if (param == "phe_influence")
                phe_influence_ = (double)val;
            else if (param == "heu_influence")
                heu_influence_ = (double)val;
            else if (param == "threshold")
                threshold_ = (double)val;
            else if (param == "epochs_count")
                epochs_count_ = (unsigned)val;
            else if (param == "ants_count")
            {
                ants_count_ = (unsigned)val;
                ants_ = std::vector<ArtificialAnt>(ants_count_);
            }
            else if (param == "best_count")
                best_count_ = (unsigned)val;
            else
                IterativeOptimization::setParams({{param, val}});
        }
    }

    ParamSet AntColonySystem::getParams() const
    {
        auto params = IterativeOptimization::getParams();
        params["evaporation"] = evaporation_;
        params["phe_decay"] = phe_decay_;
        params["phe_influence"] = phe_influence_;
        params["heu_influence"] = heu_influence_;
        params["threshold"] = threshold_;
        params["epochs_count"] = epochs_count_;
        params["ants_count"] = ants_count_;
        params["best_count"] = best_count_;
        return params;
    }
}
