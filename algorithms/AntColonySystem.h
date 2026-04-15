#pragma once

#include "IterativeOptimization.h"
#include "LayeredSchedule.h"
#include "BufferIndex.h"

#include <limits>
#include <vector>

namespace scheduling_problem::algorithms
{
/**
 * @brief Ant Colony System for scheduling.
 *
 * Builds layered multiprocessor states and evaluates them through the
 * common LayeredState decoder.
 */
class AntColonySystem : public IterativeOptimization
{
public:
    using Matrix = std::vector<std::vector<double>>;

    struct GraphContext
    {
        const Graph *graph = nullptr;
        unsigned processors = 1;
        weight_t memory_limit = std::numeric_limits<weight_t>::max();
        std::vector<std::vector<size_t>> children;
        std::vector<unsigned> indeg_template;
        std::vector<weight_t> durations;
        std::vector<weight_t> vertex_weights;
        std::vector<double> criticality;
        std::vector<double> memory_bias;
        additionals::BufferIndex buffer_index;
        std::vector<std::vector<std::pair<size_t, int>>> incoming_groups;
        bool memory_constrained = false;
    };

    struct Route
    {
        LayeredState state;
        LayeredEval eval;
        std::vector<unsigned> proc_choice;
        std::vector<size_t> prev_on_proc;
        double score = std::numeric_limits<double>::max();
    };

    /**
     * @brief Artificial ant building a layered route.
     */
    struct ArtificialAnt
    {
        Route makeRoute(const GraphContext &context,
                        double overflow_penalty,
                        const Matrix &proc_matrix,
                        const Matrix &order_matrix,
                        double phe_influence,
                        double heu_influence,
                        double threshold,
                        randgen &rng) const;
    };

    AntColonySystem(BaseOptimization &baseline = BASELINE,
                    double evaporation = 0.1,
                    double phe_decay = 0.2,
                    double phe_influence = 0.6902652173894501,
                    double heu_influence = 1.1372818452511648,
                    double threshold = 0.9859816765342961,
                    unsigned epochs_count = 240,
                    unsigned ants_count = 6,
                    unsigned best_count = 2,
                    unsigned saturation = 300,
                    double improvement = 0.5,
                    unsigned seed = 42,
                    const std::string &label = "aco");

    void setParams(const ParamSet &params) override;
    ParamSet getParams() const override;

protected:
    Schedule schedule_(const Graph &graph, const Schedule &base_schedule) override;

private:
    std::vector<Route> completeEpoch(const GraphContext &context,
                                     Matrix &proc_matrix,
                                     Matrix &order_matrix,
                                     double init_trail,
                                     double reward_scale);

    static Matrix makeMatrix(size_t rows, size_t cols, double val);

    void localPheUpdate(const Route &route,
                        Matrix &proc_matrix,
                        Matrix &order_matrix,
                        double delta) const;

    void globalPheUpdate(const std::vector<Route> &epoch_routes,
                         Matrix &proc_matrix,
                         Matrix &order_matrix,
                         double reward_scale) const;

    std::unique_ptr<BaseOptimization> copy() const override;

private:
    /** Pheromone evaporation factor rho. */
    double evaporation_ = 0.1;
    /** Local decay factor phi. */
    double phe_decay_ = 0.2;
    /** Pheromone influence alpha. */
    double phe_influence_ = 0.6902652173894501;
    /** Heuristic influence beta. */
    double heu_influence_ = 1.1372818452511648;
    /** Greedy threshold q0. */
    double threshold_ = 0.9859816765342961;
    /** Number of epochs to run. */
    unsigned epochs_count_ = 240;
    /** Number of ants per epoch. */
    unsigned ants_count_ = 6;
    /** Elite pool size. */
    unsigned best_count_ = 2;
    /** Number of processors in layered schedule model. */
    unsigned processors_ = 1;
    /** Hard memory limit for layered evaluation. */
    weight_t memory_limit_ = std::numeric_limits<weight_t>::max();
    /** Penalty multiplier for memory overflow. */
    double overflow_penalty_ = 1e6;

    /** Ant population reused across epochs. */
    std::vector<ArtificialAnt> ants_;
    /** RNG used by ants and sampling. */
    randgen rng_{42};
};

}
