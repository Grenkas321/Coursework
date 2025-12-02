#include "BaseOptimization.h"

namespace scheduling_problem::algorithms
{
    /**
     * Construct a base optimizer with a human-readable label.
     *
     * @param label  Name used in logs/outputs.
     */
    BaseOptimization::BaseOptimization(const std::string &label)
        : label_(label), duration_(0)
    {
    }

    /**
     * Public entry point: run scheduling and measure wall-clock duration.
     * Wraps the virtual schedule_() implemented by derived classes.
     *
     * @param graph  Input task graph.
     * @return Constructed schedule.
     */
    Schedule BaseOptimization::schedule(const Graph &graph)
    {
        auto start = currentTime();

        auto solution = this->schedule_(graph);

        auto stop = currentTime();
        duration({start, stop});
        return solution;
    }

    /**
     * Get a monotonic time point suitable for duration measurements.
     *
     * @return Current steady_clock time point.
     */
    BaseOptimization::TimePoint BaseOptimization::currentTime() const
    {
        return std::chrono::steady_clock::now();
    }

    /**
     * Set the last run duration from a [start, stop] segment.
     *
     * @param segment  Pair {start, stop} time points.
     */
    void BaseOptimization::duration(const std::pair<TimePoint, TimePoint> &segment)
    {
        duration_ = std::chrono::duration_cast<std::chrono::microseconds>(segment.second - segment.first).count();
    }

    /**
     * Get the last measured duration of schedule().
     *
     * @return Duration in microseconds.
     */
    long long BaseOptimization::duration() const
    {
        return duration_;
    }

    /**
     * Get the optimizer label.
     *
     * @return Label string.
     */
    const std::string &BaseOptimization::label() const
    {
        return label_;
    }

    /**
     * Set base-optimizer parameters from a key-value set.
     * Recognized key:
     * - "label": std::string — sets the optimizer label.
     *
     * @param params  Parameter map.
     */
    void BaseOptimization::setParams(const ParamSet &params)
    {
        for (auto &param : params)
            if (param.first == "label")
                label_ = (std::string)param.second;
    }

    /**
     * Get the current base-optimizer parameters as a key-value set.
     * Contains:
     * - "label": std::string
     *
     * @return Parameter map.
     */
    ParamSet BaseOptimization::getParams() const
    {
        return {{"label", label_}};
    }
}
