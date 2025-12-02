#include "ProgressBar.h"
#include <iostream>

namespace scheduling_problem::additionals
{
    /**
     * @brief Construct a textual progress bar.
     *
     * @param bar_width Number of characters used to render the bar.
     * @param count     Total number of steps (calls to show) to reach 100%.
     */
    ProgressBar::ProgressBar(unsigned bar_width,
                             unsigned count)
        : bar_width_(bar_width), count_(count), step_(0), progress_(0.0)
    {
    }

    /**
     * @brief Advance and render the progress bar to stdout.
     *
     * Prints a single-line bar with a moving '>' cursor and percentage,
     * using carriage return to update in place. On the final step, prints
     * a trailing newline. Safe to call more than @p count_ times; extra
     * calls will simply continue incrementing internal counters.
     */
    void ProgressBar::show()
    {
        if (step_ && step_ <= count_)
        {
            std::cout << "[";
            unsigned pos = (unsigned)(bar_width_ * progress_);
            for (unsigned i(0); i < bar_width_; i++)
                if (i < pos)
                    std::cout << "=";
                else if (i == pos)
                    std::cout << ">";
                else
                    std::cout << " ";
            std::cout << "] " << unsigned(progress_ * 100.0f) << "%\r";
            std::cout.flush();
        }
        if (step_ == count_)
        {
            std::cout << std::endl;
        }
        progress_ += 1.f / count_;
        step_++;
    }

    /**
     * @brief Reset the progress bar to its initial state.
     */
    void ProgressBar::reset()
    {
        progress_ = 0.0;
        step_ = 0;
    }
}
