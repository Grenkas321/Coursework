#include "ScheduleCorrector.h"

namespace scheduling_problem::algorithms
{

    ScheduleCorrector::ScheduleCorrector(unsigned seed,
                                         bool track_trail)
        : rng_(seed), moved_vid_(-1), last_pos_(-1), track_trail_(track_trail), trail_()
    {
    }

    weight_t ScheduleCorrector::transform(const Graph &graph, ScheduleStatus &status)
    {
        if (!track_trail_)
            trail_.clear();

        auto [moved_vid, tpos] = choice(graph, status);
        if (status.contains(moved_vid))
        {
            last_pos_ = status.loc(moved_vid);
            moved_vid_ = moved_vid;
            status.move(moved_vid, tpos, graph);
            // trail_.push_back({ moved_vid, last_pos });
        }
        return status.cost();
    }

    weight_t ScheduleCorrector::invtransform(const Graph &graph, ScheduleStatus &status)
    {
        if (status.contains(moved_vid_) && last_pos_ < status.upper(moved_vid_, graph))
            status.move(moved_vid_, last_pos_, graph);
        return status.cost();
    }

    std::pair<size_t, size_t> ScheduleCorrector::choice(const Graph &graph, const ScheduleStatus &status)
    {
        std::vector<size_t> moveables;

        for (const auto &job : status)
            if (status.upper(job.id, graph) > status.lower(job.id, graph) + 1)
                moveables.push_back(job.id);

        if (!moveables.size())
            return {-1, -1};

        auto index = std::uniform_int_distribution<size_t>(0, moveables.size() - 1)(rng_);
        auto vid = moveables[index];
        auto curr_pos = status.loc(vid);
        auto tpos = curr_pos;
        auto lower(status.lower(vid, graph)), upper(status.upper(vid, graph));
        while (tpos == curr_pos)
            tpos = std::uniform_int_distribution<size_t>(lower, upper - 1)(rng_);

        return {vid, tpos};
    }
}