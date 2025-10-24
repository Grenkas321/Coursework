#include "RandomSearch.h"
#include "ScheduleCorrector.h"

namespace scheduling_problem::algorithms
{
    RandomSearch::RandomSearch(unsigned max_iters,
                               unsigned saturation,
                               double improvement,
                               unsigned seed,
                               const std::string &label)
        : IterativeOptimization(BASELINE, saturation, improvement, seed, label), max_iters_(max_iters)
    {
    }

    Schedule RandomSearch::schedule_(const Graph &graph, const Schedule &base_schedule)
    {
        ScheduleStatus status_base(graph, base_schedule);
        auto best_schedule = status_base;
        cost_dynamics_.clear();
        cost_dynamics_.push_back(best_schedule.cost());
        ScheduleStatus status(graph, best_schedule);
        ScheduleCorrector corrector(seed_);
        unsigned stagnations(0);
        for (iters_count_ = 0; iters_count_ < max_iters_; iters_count_++)
        {
            corrector.transform(graph, status);
            if (status.cost() < best_schedule.cost())
            {
                auto improvement = ((double)best_schedule.cost() - status.cost()) / best_schedule.cost();
                if (improvement > improvement_)
                    stagnations = 0;
                best_schedule = status;
            }
            stagnations++;
            if (saturation_ && stagnations >= saturation_)
                break;
            cost_dynamics_.push_back(status.cost());
        }
        return best_schedule;
    }

    size_t RandomSearch::choice(const Graph &graph, const ScheduleStatus &status, size_t curr_vid)
    {
        auto lower(status.lower(curr_vid, graph)), upper(status.upper(curr_vid, graph));
        if (lower == upper)
            return upper;
        return std::uniform_int_distribution<size_t>(lower, upper - 1)(rng_);
    }

    std::unique_ptr<BaseOptimization> RandomSearch::copy() const
    {
        return std::unique_ptr<RandomSearch>(new RandomSearch(max_iters_, saturation_, improvement_, seed_, label_));
    }

    void RandomSearch::setParams(const ParamSet &params)
    {
        for (auto [param, val] : params)
        {
            if (param == "max_iters")
                max_iters_ = (unsigned)val;
        }
    }

    ParamSet RandomSearch::getParams() const
    {
        auto params = IterativeOptimization::getParams();
        params["max_iters"] = max_iters_;
        return params;
    }

    Schedule RandomSearch::generateRandomSchedule(const Graph &graph)
    {
        Schedule best_solution;
        auto n_vertex = boost::num_vertices(graph);
        for (iters_count_ = 0; iters_count_ < max_iters_; iters_count_++)
        {
            ScheduleStatus status(graph);
            for (size_t curr_vid = 0; curr_vid < n_vertex; curr_vid++)
            {
                auto pos = choice(graph, status, curr_vid);
                status.insert(curr_vid, pos, graph);
            }

            if (status.cost() < best_solution.cost() || !best_solution.size())
            {
                best_solution = status;
            }
        }
        return best_solution;
    }
}
