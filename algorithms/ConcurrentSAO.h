#pragma once
#include "BaseOptimization.h"
#include "RandomSearch.h"
#include "Greedy.h"
#include "SimulatedAnnealing.h" // для ReductionRules
#include <future>
#include <unordered_map>
#include <memory>

namespace scheduling_problem::algorithms {

class ConcurrentSAO : public BaseOptimization {
public:
    std::vector<std::vector<long long>> conveyor;

    using ParamSet = std::unordered_map<std::string, parameter>;

    ConcurrentSAO(unsigned partitions_count,
                  unsigned rsearch_iters,
                  bool warm_start,
                  bool subareas,
                  double min_temp,
                  double max_temp,
                  SimulatedAnnealing::ReductionRules reduction_rule,
                  unsigned saturation,
                  double improvement,
                  unsigned seed,
                  const std::string& label = "CSAO");

    ConcurrentSAO();

    ~ConcurrentSAO() override;

    std::unique_ptr<BaseOptimization> copy() const override;
    void setParams(const ParamSet& params) override;
    ParamSet getParams() const override;

protected:
    Schedule schedule_(const Graph& graph) override;

private:
    std::vector<ScheduleStatus> wave_(const Graph& graph,
                                      std::shared_ptr<std::vector<Schedule>> baselines,
                                      unsigned keep_top);

private:
    unsigned partitions_count_ = 3;
    unsigned rsearch_iters_    = 20;
    bool     warm_start_       = true;
    bool     subareas_         = true;

    ParamSet algo_params_; // min_temp, max_temp, reduction_rule, saturation, improvement, ...

    randgen rng_{42};
};

} // namespace scheduling_problem::algorithms
