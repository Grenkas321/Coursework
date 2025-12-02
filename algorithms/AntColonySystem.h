#pragma once
#include "IterativeOptimization.h"
#include "Greedy.h"
#include "RandomSearch.h"
#include <random>
#include <vector>

namespace scheduling_problem::algorithms {

/**
 * Ant Colony System (ACS) optimizer for task-graph scheduling.
 *
 * Builds schedules by iteratively inserting vertices according to a trade-off
 * between pheromone trails and a greedy heuristic. Supports iterative running
 * with progress tracking (via IterativeOptimization).
 */
class AntColonySystem : public IterativeOptimization {
protected:
    /** Square matrix of pheromone values τ[u][v]. */
    using Matrix = std::vector<std::vector<double>>;

    /**
     * A single artificial ant that constructs a route.
     * Inherits the greedy heuristic helpers from Greedy.
     */
    class ArtificialAnt : public Greedy {
    public:
        /**
         * Build a full schedule by inserting vertices one by one.
         *
         * @param graph          Task graph.
         * @param matrix         Pheromone matrix τ (updated locally per step).
         * @param phe_influence  α, pheromone influence.
         * @param heu_influence  β, heuristic influence.
         * @param threshold      q0, probability of greedy choice.
         * @param rng            Random generator.
         * @return Completed schedule status.
         */
        ScheduleStatus makeRoute(const Graph& graph,
                                 Matrix& matrix,
                                 double phe_influence,
                                 double heu_influence,
                                 double threshold,
                                 randgen& rng);

        /**
         * Choose insertion position for the current vertex.
         * Uses ACS rule: greedy argmax with prob q0, else roulette by desirability.
         *
         * @param graph          Task graph.
         * @param matrix         Pheromone matrix τ.
         * @param status         Current partial schedule.
         * @param curr_vid       Vertex id to insert.
         * @param phe_influence  α, pheromone exponent.
         * @param heu_influence  β, heuristic exponent.
         * @param threshold      q0, greedy probability.
         * @param rng            Random generator.
         * @return Insertion position (0-based).
         */
        size_t choice(const Graph& graph,
                      const Matrix& matrix,
                      const ScheduleStatus& status,
                      size_t curr_vid,
                      double phe_influence,
                      double heu_influence,
                      double threshold,
                      randgen& rng);
    };

public:
    /**
     * Default constructor (keeps default hyperparameters).
     */
    AntColonySystem() = default;

    /**
     * Convenience constructor with default ACS hyperparameters.
     *
     * @param baseline  Baseline optimization used for initial schedule/rebuilds.
     */
    explicit AntColonySystem(BaseOptimization& baseline);

    /**
     * Full-parameter constructor.
     *
     * @param baseline       Baseline optimizer for initialization/rebuilds.
     * @param evaporation    Global evaporation rate ρ in (0,1].
     * @param phe_decay      Local pheromone decay ξ in (0,1].
     * @param phe_influence  α, pheromone influence.
     * @param heu_influence  β, heuristic influence.
     * @param threshold      q0, greedy probability.
     * @param epochs_count   Number of epochs to run.
     * @param ants_count     Number of ants per epoch.
     * @param best_count     Elite pool size.
     * @param saturation     Max non-improving iterations before early stop (0 disables).
     * @param improvement    Relative improvement threshold to reset stagnation.
     * @param seed           RNG seed.
     * @param label          Human-readable label (default: "ACO").
     */
    AntColonySystem(BaseOptimization& baseline,
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
                    const std::string& label = "ACO");

    /**
     * Set hyperparameters from a key-value set.
     *
     * @param params  Map of parameter name -> value.
     */
    void setParams(const ParamSet& params) override;

    /**
     * Get current hyperparameters as a key-value set.
     *
     * @return Parameter map including ACS and iterative fields.
     */
    ParamSet getParams() const override;

    /**
     * Polymorphic copy.
     *
     * @return New heap-allocated optimizer with the same parameters.
     */
    std::unique_ptr<BaseOptimization> copy() const override;

protected:
    /**
     * Core scheduling routine using ACS given an optional base schedule.
     *
     * @param graph          Input graph.
     * @param base_schedule  Starting schedule (may be empty).
     * @return Best schedule found.
     */
    Schedule schedule_(const Graph& graph, const Schedule& base_schedule) override;

    /**
     * Convenience overload that starts from an empty base schedule.
     *
     * @param g  Input graph.
     * @return Best schedule found.
     */
    Schedule schedule_(const Graph& g) override {
        Schedule empty; return schedule_(g, empty);
    }

private:
    /**
     * Create an n×n pheromone matrix prefilled with `pad`.
     *
     * @param n_vertices  Number of vertices (matrix dimension).
     * @param pad         Initial pheromone value.
     * @return Matrix τ of size n×n.
     */
    Matrix makeMatrix(size_t n_vertices, double pad) const;

    /**
     * Run one epoch: every ant builds a route, applies local update,
     * and routes are returned sorted by cost (ascending).
     *
     * @param graph       Task graph.
     * @param matrix      Pheromone matrix (locally updated).
     * @param init_trail  Baseline trail value used in local updates.
     * @return Routes produced in this epoch.
     */
    std::vector<ScheduleStatus> completeEpoch(const Graph& graph,
                                              Matrix& matrix,
                                              double init_trail);

    /**
     * Local pheromone update (ACS): τ := (1 - ξ) * τ + ξ * delta.
     *
     * @param status  Route just built.
     * @param matrix  Pheromone matrix to update.
     * @param delta   Baseline trail value.
     */
    void localPheUpdate(const ScheduleStatus& status, Matrix& matrix, double delta) const;

    /**
     * Global pheromone update:
     * evaporate all entries, then reinforce entries used by epoch routes
     * proportionally to ρ * (weight_max / cost(route)).
     *
     * @param epoch_routes  Routes from the current epoch.
     * @param matrix        Pheromone matrix to update.
     * @param weight_max    Max vertex weight (scaling factor).
     */
    void globalPheUpdate(const std::vector<ScheduleStatus>& epoch_routes,
                         Matrix& matrix,
                         double weight_max) const;

private:
    /** Global evaporation rate ρ. */
    double   evaporation_   = 0.1;
    /** Local pheromone decay ξ. */
    double   phe_decay_     = 0.2;
    /** Pheromone influence α. */
    double   phe_influence_ = 0.4;
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
