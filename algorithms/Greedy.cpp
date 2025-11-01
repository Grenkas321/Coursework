#include "Greedy.h"
#include <boost/range/irange.hpp>
#include <boost/range/adaptor/reversed.hpp>

namespace scheduling_problem::algorithms
{
    /**
     * Baseline greedy scheduler
     */
    Greedy BASELINE;

    /**
     * @brief Construct algorithm instance
     */
    Greedy::Greedy(const std::string &label) : BaseOptimization(label)
    {
    }

    /**
     * @brief Constructs a schedule for a given graph
     */
    Schedule Greedy::schedule_(const Graph &graph)
    {
        ScheduleStatus schedule(graph);
        auto num_vertices = boost::num_vertices(graph);
        for (size_t curr_vid(0); curr_vid < num_vertices; curr_vid++)
        {
            auto pos = choice(graph, schedule, curr_vid);
            schedule.insert(curr_vid, pos, graph);
        }
        return schedule;
    }

    /**
     * @brief Makes copy of algorithm instance and casts it to base class
     */
    std::unique_ptr<BaseOptimization> Greedy::copy() const
    {
        return std::unique_ptr<Greedy>(new Greedy(label_));
    }

    /**
     * @brief Chooses insert position for task using heuristic information
     */
    size_t Greedy::choice(const Graph &graph,
                          const ScheduleStatus &schedule,
                          size_t curr_vid)
    {
        auto heu_info = heuInfo(graph, schedule, curr_vid);
        size_t pos = std::max_element(heu_info.begin(), heu_info.end(),
                                      [](auto &prob_1, auto &prob_2)
                                      { return prob_1.second < prob_2.second; })
                         ->first;
        return pos;
    }

    /**
     * @brief Computes heuristic information for every available insert position
     *
     * Heuristic info is 1 / f_pos where f_pos is goal function value for
     * schedule in which the given task is inserted in position pos.
     */
    std::unordered_map<size_t, double> Greedy::heuInfo(const Graph &graph,
                                                       const ScheduleStatus &status,
                                                       size_t curr_vid)
    {
        if (!status.size())
            return {{0, 1.}}; // only one available position

        std::unordered_map<size_t, double> heu_info;
        auto lower(status.lower(curr_vid, graph));
        auto poscount(status.size() + 1 - lower);
        std::vector<weight_t> targets(poscount);

        // stable cost part
        weight_t stable_cost(0), stable_target(0);
        for (size_t pos(0); pos < lower; pos++)
        {
            stable_target += status[pos].volume;
            if (stable_target > stable_cost)
                stable_cost = stable_target;
            stable_target -= status[pos].release;
        }

        // fill targets
        weight_t target(0), curr_weight(boost::get(vertex_weight_t(), graph, curr_vid));
        for (size_t pos(0); pos < status.size(); pos++)
        {
            target += status[pos].volume;
            if (pos >= lower)
                targets[pos - lower] = target;
            target -= status[pos].release;
        }
        targets[poscount - 1] = target + curr_weight;

        // calculate heu_info for the last position
        heu_info[status.size()] = 1. / std::max({stable_cost, *std::max_element(targets.begin(), targets.end())});

        // find released parents and their last successors
        size_t last_child, child_remain;
        weight_t curr_release(boost::out_degree(curr_vid, graph) ? 0 : curr_weight);
        std::vector<std::pair<size_t, weight_t>> released; // { last_child, weight }
        for (const auto &parent : status.parents(curr_vid, graph))
        {
            std::tie(child_remain, last_child) = status.releaseOn(parent, graph);
            if (child_remain == 1)
            {
                auto pweight = boost::get(vertex_weight_t(), graph, parent);
                released.push_back({last_child, pweight});
                curr_release += pweight;
            }
        }
        std::sort(released.begin(), released.end(),
                  [](auto &rel1, auto &rel2)
                  { return rel1.first < rel2.first; });

        // pseudo insert curr_vid on available positions and calculate statistics
        weight_t right_max_target(0), left_max_target(0);
        auto places = boost::adaptors::reverse(boost::irange(lower, status.size()));
        for (const auto &curr_pos : places)
        {
            // update curr_release
            for (const auto &rel : released)
                if (rel.first == curr_pos)
                    curr_release -= rel.second;
                else if (rel.first > curr_pos)
                    break;

            // update targets
            auto next_pos = curr_pos - lower + 1;
            targets[curr_pos - lower] = targets[curr_pos - lower] - status[curr_pos].volume + curr_weight;
            targets[next_pos] = targets[curr_pos - lower] - curr_release + status[curr_pos].volume;
            if (targets[next_pos] > right_max_target)
                right_max_target = targets[next_pos];

            // calculate heu_info for curr_pos position
            left_max_target = *std::max_element(targets.begin(), targets.begin() + next_pos);
            heu_info[curr_pos] = 1. / std::max({left_max_target, right_max_target});
        }

        return heu_info;
    }
}
