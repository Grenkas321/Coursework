#pragma once

#include "Schedule.h"

namespace scheduling_problem::additionals
{
    /**
     * Checks schedule correctness for the given graph
     */
    class ScheduleChecker
    {
    private:
        bool logging_;
        std::string info_;
        std::string dump_path_;

    public:
        /**
         * Constructor
         */
        ScheduleChecker(bool logging = false, std::string dump_path = "");

        /**
         * Information about checker
         */
        std::string info();
        /**
         * Check correctness of the schedule
         */
        bool isCorrect(const Graph &graph, const Schedule &schedule);
        /**
         * Check correctness of the schedule
         */
        bool isCorrect(const Graph &graph, const nlohmann::ordered_json &schedule);
        /**
         * Check correctness of the schedule
         */
        bool isCorrect(const Graph &graph, const std::string &schedule_path);
    };
}
