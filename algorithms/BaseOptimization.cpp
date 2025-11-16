#include "BaseOptimization.h"

namespace scheduling_problem::algorithms
{
    /**
     * @brief Initialize basic algorithm parameters
     */
    BaseOptimization::BaseOptimization(const std::string &label)
        : label_(label), duration_(0)
    {
    }

    /**
     * @brief Decorator method that performs graph scheduling with measurement of construction time
     */
    Schedule BaseOptimization::schedule(const Graph &graph)
    {
        auto start = currentTime();
        std::cout << "this->schedule_(graph) start" << std::endl;
        auto solution = this->schedule_(graph);
        std::cout << "this->schedule_(graph) stop" << std::endl;
        auto stop = currentTime();
        duration({start, stop});
        return solution;
    }

    BaseOptimization::TimePoint BaseOptimization::currentTime() const
    {
        return std::chrono::steady_clock::now();
    }

    void BaseOptimization::duration(const std::pair<TimePoint, TimePoint> &segment)
    {
        duration_ = std::chrono::duration_cast<std::chrono::microseconds>(segment.second - segment.first).count();
    }

    long long BaseOptimization::duration() const
    {
        return duration_;
    }

    const std::string &BaseOptimization::label() const
    {
        return label_;
    }

    /**
     * @brief Set algorithm parameters
     */
    void BaseOptimization::setParams(const ParamSet &params)
    {
        for (auto &param : params)
            if (param.first == "label")
                label_ = (std::string)param.second;
    }

    /**
     * @brief Get algorithm parameters
     */
    ParamSet BaseOptimization::getParams() const
    {
        return {{"label", label_}};
    }
}
