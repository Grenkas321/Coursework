#pragma once

#include <chrono>
#include <unordered_map>
#include "general_types.h"
#include "Schedule.h"
#include "parameter.h"

namespace scheduling_problem::algorithms
{
    /**
     * Map of algorithm hyperparameters: name -> value.
     */
    typedef std::unordered_map<std::string, parameter> ParamSet;

    /**
     * Abstract base class for all optimization algorithms.
     *
     * Provides:
     * - a timed public `schedule()` wrapper that measures wall-clock time,
     * - a label for logging/outputs,
     * - virtual Parameter I/O (`setParams`, `getParams`),
     * - a polymorphic `copy()` constructor,
     * - a protected virtual `schedule_()` to implement in derived classes.
     */
    class BaseOptimization
    {
    protected:
        /** Monotonic time point for duration measurement. */
        typedef std::chrono::steady_clock::time_point TimePoint;

        /** Last measured duration of `schedule()` in microseconds. */
        long long duration_;
        /** Human-readable algorithm name/label. */
        std::string label_;

    public:
        /**
         * Construct a base optimizer with an optional label.
         *
         * @param label  Algorithm name used in logs/outputs (default: "base").
         */
        BaseOptimization(const std::string& label = "base");

        /**
         * Copy constructor (defaulted).
         */
        BaseOptimization(const BaseOptimization& other) = default;

        /**
         * Run the algorithm on a given graph and measure execution time.
         * Calls the overridden `schedule_()` implemented by subclasses.
         *
         * @param graph  Input task graph.
         * @return Constructed schedule.
         */
        Schedule schedule(const Graph& graph);

        /**
         * Set algorithm parameters from a key-value map.
         * Base class recognizes: "label" (std::string).
         * Subclasses may override to handle more keys.
         *
         * @param params  Parameter set.
         */
        virtual void setParams(const ParamSet& params);

        /**
         * Get current parameters as a key-value map.
         * Base class returns: {"label": <string>}.
         *
         * @return Parameter set.
         */
        virtual ParamSet getParams() const;

        /**
         * Polymorphic copy constructor.
         *
         * @return Newly allocated optimizer with the same configuration.
         */
        virtual std::unique_ptr<BaseOptimization> copy() const = 0;

        /**
         * Get the last measured duration of `schedule()`.
         *
         * @return Duration in microseconds.
         */
        long long duration() const;

        /**
         * Get the algorithm label.
         *
         * @return Label string.
         */
        const std::string& label() const;

        /**
         * Virtual destructor.
         */
        virtual ~BaseOptimization() = default;

    protected:
        /**
         * Core scheduling routine to be implemented by derived classes.
         * Invoked by the timed public wrapper `schedule()`.
         *
         * @param graph  Input task graph.
         * @return Constructed schedule.
         */
        virtual Schedule schedule_(const Graph& graph) = 0;

        /**
         * Get the current monotonic time point.
         *
         * @return Steady clock time point.
         */
        TimePoint currentTime() const;

        /**
         * Store the duration based on a time segment [start, stop].
         *
         * @param segment  Pair {start, stop} time points.
         */
        void duration(const std::pair<TimePoint, TimePoint>& segment);
    };
}
