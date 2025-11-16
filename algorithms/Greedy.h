#pragma once
#include "BaseOptimization.h"
#include "GreedyHeuristics.h"
#include "ScheduleStatus.h"
#include <unordered_map>

namespace scheduling_problem::algorithms {

class Greedy : public BaseOptimization {
public:
    explicit Greedy(const std::string& label = "Greedy");

    std::unique_ptr<BaseOptimization> copy() const override;

protected:
    Schedule schedule_(const Graph& graph) override;

    size_t choice(const Graph& graph, const ScheduleStatus& schedule, size_t curr_vid);
    std::unordered_map<size_t,double> heuInfo(const Graph& graph,
                                              const ScheduleStatus& status,
                                              size_t curr_vid);
};

} // namespace scheduling_problem::algorithms
