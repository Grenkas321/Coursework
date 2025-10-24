#pragma once

#include <random>
#include <vector>
#include "general_types.h"
#include "ScheduleStatus.h"

namespace scheduling_problem::algorithms
{
    /**
     * Corrects the given schedule in order to match the given graph
     */
    class ScheduleCorrector
    {
    protected:
        std::mt19937 rng_;
        size_t moved_vid_, last_pos_;
        bool track_trail_;
        std::vector<std::pair<size_t, size_t>> trail_;

    public:
        /**
         * Class constructor
         */
        ScheduleCorrector(unsigned seed = 42, bool track_trail = false);
        /**
         * Transform schedule to match the given graph and count its goal function
         */
        weight_t transform(const Graph &graph, ScheduleStatus &status);
        /**
         * Perform inverted transformation and count the goal function of a transformed schedule
         */
        weight_t invtransform(const Graph &graph, ScheduleStatus &status);

    private:
        /**
         * Choose next node to move and its source and destination positions
         */
        std::pair<size_t, size_t> choice(const Graph &graph, const ScheduleStatus &status);
    };
}