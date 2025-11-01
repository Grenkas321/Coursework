#include "ProgressBar.h"
#include <iostream>

namespace scheduling_problem::additionals
{
    /**
     * Constructor
     */
    ProgressBar::ProgressBar(unsigned bar_width,
                             unsigned count)
        : bar_width_(bar_width), count_(count), step_(0), progress_(0.0)
    {
    }
    /**
     * View the progress bar
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
     * Reset progress bar to zero
     */
    void ProgressBar::reset()
    {
        progress_ = 0.0;
        step_ = 0;
    }
}
