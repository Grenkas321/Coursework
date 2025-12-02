#include "ScheduleChecker.h"

#include <unordered_map>
#include <iterator>
#include <sstream>
#include <limits>
#include <boost/range/iterator_range.hpp>

namespace scheduling_problem::additionals
{
    using scheduling_problem::edge_buffer_id_t;
    using scheduling_problem::weight_t;

    /**
     * @brief Hash functor for pairs (parent_id, buffer_id) used as keys in unordered_map.
     */
    struct PairHash {
        size_t operator()(const std::pair<size_t,int>& p) const noexcept {
            return std::hash<size_t>{}(p.first)
                 ^ (static_cast<size_t>(p.second) + 0x9e3779b97f4a7c15ULL + (p.first<<6) + (p.first>>2));
        }
    };

    /**
     * @brief Core buffer-based schedule validation and peak computation.
     *
     * Logic:
     *  1) Initialize a counter of remaining consumers for each buffer group keyed by (parent, buffer_id).
     *  2) Iterate the schedule in order. Each job increases the current target by its volume.
     *     Track the running maximum as @p peak.
     *  3) For every incoming edge to the scheduled job, decrement the remaining-consumers counter
     *     for the corresponding (parent, buffer_id). When the counter reaches zero, add exactly the
     *     buffer weight of that edge to the release for this position.
     *  4) Verify that the computed release equals the recorded release in the schedule entry.
     *
     * On mismatch or internal inconsistency, the function writes a message to @p msg (if provided) and returns false.
     * If @p out_peak is provided, it is set to the maximum observed target during the scan.
     *
     * @param graph     Input graph with edge_buffer_id_t and edge weights.
     * @param sched     Schedule to verify (sequence of jobs with .id, .volume, .release).
     * @param msg       Optional error message output.
     * @param out_peak  Optional output for the peak target value observed.
     * @return true if the schedule passes validation, false otherwise.
     */
    static bool check_by_buffers_impl(const Graph& graph,
                                      const Schedule& sched,
                                      std::string* msg,
                                      weight_t* out_peak = nullptr)
    {
        std::unordered_map<std::pair<size_t,int>, size_t, PairHash> remain;

        for (auto e : boost::make_iterator_range(boost::edges(graph)))
        {
            auto parent = boost::source(e, graph);
            int bid = boost::get(edge_buffer_id_t(), graph, e);
            remain[{static_cast<size_t>(parent), bid}]++;
        }

        weight_t target = 0;
        weight_t peak   = 0;

        for (size_t pos = 0; pos < sched.size(); ++pos)
        {
            const auto& job = sched[pos];

            target += job.volume;
            if (target > peak) peak = target;

            weight_t release = 0;
            for (auto parent : boost::make_iterator_range(boost::inv_adjacent_vertices(job.id, graph)))
            {
                auto pr = boost::edge(parent, job.id, graph);
                if (!pr.second) continue;
                auto e = pr.first;

                int  bid = boost::get(edge_buffer_id_t(), graph, e);
                auto key = std::make_pair(static_cast<size_t>(parent), bid);

                auto it = remain.find(key);
                if (it != remain.end() && it->second > 0)
                {
                    if (--(it->second) == 0) {
                        release += boost::get(boost::edge_weight, graph, e);
                    }
                }
                else
                {
                    if (msg) *msg = "Internal error: negative or missing buffer consumers for (parent="
                                    + std::to_string(static_cast<size_t>(parent))
                                    + ", bid=" + std::to_string(bid) + ") at pos " + std::to_string(pos);
                    return false;
                }
            }

            if (release != job.release)
            {
                if (msg) {
                    std::ostringstream oss;
                    oss << "Release mismatch at position " << pos
                        << " (job.id=" << job.id << "): expected " << release
                        << ", got " << job.release;
                    *msg = oss.str();
                }
                return false;
            }

            target -= release;

            if (job.id != static_cast<size_t>(-10000) && job.id != static_cast<size_t>(10000)) {
                /* invariant checks can be added here if needed */
            }
        }

        if (out_peak) *out_peak = peak;
        return true;
    }

    /**
     * @brief Validate schedule against buffer-consumer semantics.
     *
     * @param graph    Input graph.
     * @param sched    Schedule to validate.
     * @param message  Optional error description on failure.
     * @return true if the schedule is consistent; false otherwise.
     */
    bool ScheduleChecker::isCorrect(const Graph& graph,
                                    const Schedule& sched,
                                    std::string* message)
    {
        weight_t dummy_peak = 0;
        return check_by_buffers_impl(graph, sched, message, &dummy_peak);
    }

    /**
     * @brief Recompute the peak target (maximum in-flight buffer volume) for a schedule.
     *
     * @param graph  Input graph.
     * @param sched  Schedule to analyze.
     * @return Peak value encountered during the schedule execution.
     */
    weight_t ScheduleChecker::recomputePeak(const Graph& graph, const Schedule& sched)
    {
        weight_t peak = 0;
        std::string ignored;
        check_by_buffers_impl(graph, sched, &ignored, &peak);
        return peak;
    }
}
