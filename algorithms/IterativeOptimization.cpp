#include "IterativeOptimization.h"

namespace scheduling_problem::algorithms
{

    IterativeOptimization::IterativeOptimization(const BaseOptimization &baseline,
                                                 unsigned saturation,
                                                 double improvement,
                                                 unsigned seed,
                                                 const std::string &label)
        : BaseOptimization(label), baseline_(std::move(baseline.copy())), saturation_(saturation), improvement_(improvement), seed_(seed), rng_(seed), iters_count_(0)
    {
    }

    Schedule IterativeOptimization::schedule_(const Graph &graph)
    {
        cost_dynamics_.clear();
        auto base_schedule = baseline_->schedule(graph);
        auto solution = schedule_(graph, base_schedule);

        return solution;
    }

    Schedule IterativeOptimization::schedule(const Graph &graph, const Schedule &base_schedule)
    {
        cost_dynamics_.clear();
        auto start = currentTime();
        auto solution = schedule_(graph, base_schedule);
        auto stop = currentTime();
        duration({start, stop});
        return solution;
    }

    unsigned IterativeOptimization::itersCount() const
    {
        return iters_count_;
    }

    std::vector<scheduling_problem::weight_t> IterativeOptimization::costDynamics() const
    {
        return cost_dynamics_;
    }

    void IterativeOptimization::setParams(const ParamSet &params)
    {
        for (const auto &param : params)
        {
            if (param.first == "saturation")
                saturation_ = (unsigned)param.second;
            if (param.first == "improvement")
                improvement_ = param.second;
            if (param.first == "seed")
            {
                seed_ = param.second;
                rng_ = randgen(seed_);
            }
            else
                BaseOptimization::setParams({param});
        }
    }

    ParamSet IterativeOptimization::getParams() const
    {
        auto params = BaseOptimization::getParams();
        params["saturation"] = saturation_;
        params["improvement"] = improvement_;
        params["seed"] = seed_;
        return params;
    }
}
