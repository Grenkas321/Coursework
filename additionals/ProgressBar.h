#pragma once

#include "general_types.h"
#include <iostream>

namespace scheduling_problem::additionals
{
    /**
     * Tracks the execution process over the batch of input graphs
     */
    class ProgressBar
    {
    private:
        unsigned bar_width_, count_, step_;
        float progress_;

    public:
        ProgressBar(unsigned bar_width,
                    unsigned count);

        void show();
        void reset();
    };
}
