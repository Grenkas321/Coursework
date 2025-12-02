#include "ScheduleCorrector.h"

namespace scheduling_problem::algorithms
{
    /**
     * Construct a schedule corrector (random local mover).
     *
     * @param seed        RNG seed used for randomized moves.
     * @param track_trail If true, internal move trail can be recorded (currently unused).
     */
    ScheduleCorrector::ScheduleCorrector(unsigned seed,
                                         bool track_trail)
        : rng_(seed), moved_vid_(-1), last_pos_(-1), track_trail_(track_trail), trail_()
    {
    }

    /**
     * Apply a random feasible move to the schedule.
     *
     * Chooses a movable vertex and a new target position within its admissible
     * range (respecting precedence), moves it, and returns the new cost.
     * Stores the last move so it can be reverted by @c invtransform.
     *
     * @param graph   Input task graph.
     * @param status  Schedule to modify in place.
     * @return        Current schedule cost after the move.
     */
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
        }
        return status.cost();
    }

    /**
     * Revert the last move performed by @c transform (if still valid).
     *
     * If the previously moved vertex is still present and its stored position
     * is within the current admissible right bound, move it back.
     *
     * @param graph   Input task graph.
     * @param status  Schedule to modify in place.
     * @return        Current schedule cost after the revert (or no-op).
     */
    weight_t ScheduleCorrector::invtransform(const Graph &graph, ScheduleStatus &status)
    {
        if (status.contains(moved_vid_) && last_pos_ < status.upper(moved_vid_, graph))
            status.move(moved_vid_, last_pos_, graph);
        return status.cost();
    }

    /**
     * Randomly select a movable vertex and a different feasible target position.
     *
     * A vertex is considered movable if its admissible interval has size > 1:
     *   upper(id) > lower(id) + 1.
     * Among movable vertices, pick one uniformly at random, then sample a target
     * position uniformly from [lower, upper) excluding the current position.
     *
     * @param graph    Input task graph.
     * @param status   Current schedule (read-only).
     * @return         Pair {vertex_id, target_pos}; {-1, -1} if nothing is movable.
     */
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
