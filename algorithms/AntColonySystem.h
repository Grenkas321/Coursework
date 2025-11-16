#pragma once
#include "IterativeOptimization.h"
#include "Greedy.h"
#include "RandomSearch.h"
#include <random>
#include <vector>

namespace scheduling_problem::algorithms {

class AntColonySystem : public IterativeOptimization {
protected:
    using Matrix = std::vector<std::vector<double>>;

    class ArtificialAnt : public Greedy {
    public:
        ScheduleStatus makeRoute(const Graph& graph,
                                 Matrix& matrix,
                                 double phe_influence,
                                 double heu_influence,
                                 double threshold,
                                 randgen& rng);

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
    AntColonySystem() = default;

    explicit AntColonySystem(BaseOptimization& baseline);

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

    void setParams(const ParamSet& params) override;
    ParamSet getParams() const override;
    std::unique_ptr<BaseOptimization> copy() const override;

protected:
    Schedule schedule_(const Graph& graph, const Schedule& base_schedule) override;
    Schedule schedule_(const Graph& g) override {
        Schedule empty; return schedule_(g, empty);
    }

private:
    Matrix makeMatrix(size_t n_vertices, double pad) const;
    std::vector<ScheduleStatus> completeEpoch(const Graph& graph,
                                              Matrix& matrix,
                                              double init_trail);
    void localPheUpdate(const ScheduleStatus& status, Matrix& matrix, double delta) const;
    void globalPheUpdate(const std::vector<ScheduleStatus>& epoch_routes,
                         Matrix& matrix,
                         double weight_max) const;

private:
    double   evaporation_   = 0.1;
    double   phe_decay_     = 0.2;
    double   phe_influence_ = 0.4;
    double   heu_influence_ = 0.6;
    double   threshold_     = 0.9;
    unsigned epochs_count_  = 1000;
    unsigned ants_count_    = 10;
    unsigned best_count_    = 3;

    std::vector<ArtificialAnt> ants_;
    randgen rng_{42};
    RandomSearch rebuilder_{3000};
};

} // namespace scheduling_problem::algorithms
