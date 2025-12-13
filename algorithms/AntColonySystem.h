#pragma once

#include "IterativeOptimization.h"
#include "RandomSearch.h"
#include "Greedy.h"

#include <vector>
#include <utility>

namespace scheduling_problem::algorithms
{
/**
 * @brief Ant Colony System for scheduling.
 *
 * Produces schedules by combining pheromone trails with greedy heuristic info.
 */
class AntColonySystem : public IterativeOptimization
{
public:
    typedef std::vector<std::vector<double>> Matrix;

    /**
     * @brief Artificial ant building a route.
     *
     * Reuses Greedy::heuInfo to compute heuristic desirability of insertion positions.
     */
    struct ArtificialAnt : public Greedy
    {
        ScheduleStatus makeRoute(const Graph &graph,
                                 Matrix &matrix,
                                 double phe_influence,
                                 double heu_influence,
                                 double threshold,
                                 randgen &rng);

        size_t choice(const Graph &graph,
                      Matrix &matrix,
                      const ScheduleStatus &status,
                      size_t curr_vid,
                      double phe_influence,
                      double heu_influence,
                      double threshold,
                      randgen &rng);
    };

    AntColonySystem(BaseOptimization &baseline = BASELINE,
                    double evaporation = 0.1,
                    double phe_decay = 0.4,
                    double phe_influence = 0.4,
                    double heu_influence = 0.6,
                    double threshold = 0.9,
                    unsigned epochs_count = 1000,
                    unsigned ants_count = 10,
                    unsigned best_count = 3,
                    unsigned saturation = 0,
                    double improvement = 0.0,
                    unsigned seed = 42,
                    const std::string &label = "aco");

protected:
    virtual Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;

private:
    std::vector<ScheduleStatus> completeEpoch(const Graph &graph,
                                              Matrix &matrix,
                                              double init_trail);

    static Matrix makeMatrix(size_t dim, double val);

    void localPheUpdate(const ScheduleStatus &status,
                        Matrix &matrix,
                        double delta) const;

    void globalPheUpdate(const std::vector<ScheduleStatus> &epoch_routes,
                         Matrix &matrix,
                         double weight_max) const;

    std::unique_ptr<BaseOptimization> copy() const override;

private:
    /** Pheromone evaporation factor ρ. */
    double   evaporation_    = 0.1;
    /** Local decay factor φ. */
    double   phe_decay_      = 0.4;
    /** Pheromone influence α. */
    double   phe_influence_  = 0.4;
    /** Heuristic influence β. */
    double   heu_influence_ = 0.6;
    /** Greedy threshold q0. */
    double   threshold_     = 0.9;
    /** Number of epochs to run. */
    unsigned epochs_count_  = 1000;
    /** Number of ants per epoch. */
    unsigned ants_count_    = 10;
    /** Elite pool size. */
    unsigned best_count_    = 3;

    /** Ant population reused across epochs. */
    std::vector<ArtificialAnt> ants_;
    /** RNG used by ants and sampling. */
    randgen rng_{42};
    /** Random schedule generator used for seeding/rebuilds. */
    RandomSearch rebuilder_{3000};
};

}