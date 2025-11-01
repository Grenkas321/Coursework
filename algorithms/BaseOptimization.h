#pragma once

#include <chrono>
#include <unordered_map>
#include "general_types.h"
#include "Schedule.h"
#include "parameter.h"


namespace scheduling_problem::algorithms
{
    /**
     * @brief Dictionary of algorithm parameters
    */
    typedef std::unordered_map<std::string, parameter> ParamSet;

    /**
     * @brief Abstract base class for all optimization algorithms
    */
    class BaseOptimization
    {
    protected:

        typedef std::chrono::steady_clock::time_point TimePoint;

        long long duration_;  // algorithm execution time
        std::string label_;  // algorithm name

    public:
        /**
         * @param label algorithm name
        */
        BaseOptimization(const std::string& label = "base");
        /**
         * Copy constructor
        */
        BaseOptimization(const BaseOptimization& other) = default;

        /**
         * @param graph Input graph
         * @return Constructed schedule
        */
        Schedule schedule(const Graph& graph);

        /**
         * @param params algorithm parameters
        */
        virtual void setParams(const ParamSet& params);

        /**
         * @return Algorithm parameters
        */
        virtual ParamSet getParams() const;

        /**
         * Copy function
        */
        virtual std::unique_ptr<BaseOptimization> copy() const = 0;

        /**
         * @brief Get the computation time of the schedule
        */
        long long duration() const;

        /**
         * Get algorithm label
         * @return Algorithm label
        */
        const std::string& label() const;
        /**
         * Destructor
        */
        virtual ~BaseOptimization() = default;

    protected:

        /**
         * Constructs a schedule for a given graph
         * @param graph Input graph
         * @return Constructed schedule
        */
        virtual Schedule schedule_(const Graph& graph) = 0;

        /**
         * Get current system time
        */
        TimePoint currentTime() const;
        /**
         * Compute the duration between two time points
        */
        void duration(const std::pair<TimePoint, TimePoint>& segment);
    };
}
