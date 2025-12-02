#pragma once

#include "general_types.h"
#include <iostream>

namespace scheduling_problem::additionals
{
    /**
     * @brief Simple textual progress bar for batch processing.
     *
     * Renders a single-line bar to stdout that updates in place on each call to show().
     * After the final step, prints a trailing newline.
     */
    class ProgressBar
    {
    private:
        /** @brief Bar width in characters. */
        unsigned bar_width_;
        /** @brief Total number of steps until completion. */
        unsigned count_;
        /** @brief Current step counter. */
        unsigned step_;
        /** @brief Normalized progress in [0,1]. */
        float progress_;

    public:
        /**
         * @brief Construct a progress bar.
         *
         * @param bar_width Number of characters for the visual bar.
         * @param count     Total number of updates (steps) to reach 100%.
         */
        ProgressBar(unsigned bar_width,
                    unsigned count);

        /**
         * @brief Advance one step and render the bar to stdout.
         *
         * Prints the bar with a percentage; uses carriage return to update in place.
         * Emits a newline automatically when the last step is reached.
         */
        void show();

        /**
         * @brief Reset internal counters to the initial state.
         */
        void reset();
    };
}
