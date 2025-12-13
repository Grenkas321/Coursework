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
        : rng_(seed), moved_vid_(kInvalid), last_pos_(kInvalid), track_trail_(track_trail)

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
        if (moved_vid == kInvalid)
            return status.cost();

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
        if (moved_vid_ == kInvalid || last_pos_ == kInvalid)
            return status.cost();
        if (moved_vid_ == 0 || !status.contains(moved_vid_))
            return status.cost();

        const size_t lower = status.lower(moved_vid_, graph);
        size_t upper = status.upper(moved_vid_, graph);
        if (upper >= status.size())
            upper = status.size() - 1;

        if (last_pos_ < lower || last_pos_ > upper)
            return status.cost();

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
        moveables.reserve(status.size());

        const size_t sz = status.size();
        if (sz == 0)
            return {kInvalid, kInvalid};

        for (const auto& job : status)
        {
            auto vid = job.id;
            const size_t lower = status.lower(vid, graph);
            size_t upper = status.upper(vid, graph);
            if (upper >= sz) upper = sz - 1; // для move нельзя выходить за последний индекс
            if (upper > lower)
                moveables.push_back(vid);
        }

        if (moveables.empty())
            return {kInvalid, kInvalid};

        std::uniform_int_distribution<size_t> dist_vid(0, moveables.size() - 1);
        const size_t moved_vid = moveables[dist_vid(rng_)];

        const size_t lower = status.lower(moved_vid, graph);
        size_t upper = status.upper(moved_vid, graph);
        if (upper >= sz) upper = sz - 1;

        if (upper <= lower)
            return {kInvalid, kInvalid};

        std::uniform_int_distribution<size_t> dist_pos(lower, upper);
        size_t tpos = dist_pos(rng_);
        while (tpos == status.loc(moved_vid))
            tpos = dist_pos(rng_);

        return {moved_vid, tpos};
    }

}