#include "ScheduleChecker.h"
#include <iostream>
#include <fstream>
#include "ScheduleStatus.h"

namespace scheduling_problem::additionals
{
    ScheduleChecker::ScheduleChecker(bool logging,
                                     std::string dump_path)
        : logging_(logging), dump_path_(dump_path)
    {
    }

    std::string ScheduleChecker::info()
    {
        return info_;
    }

    bool ScheduleChecker::isCorrect(const Graph &graph, const Schedule &schedule)
    {
        ScheduleStatus status(graph, schedule);
        std::vector<int> parents(schedule.size()), children(schedule.size());
        for (auto vertex_id : boost::make_iterator_range(boost::vertices(graph)))
        {
            parents[vertex_id] = status.parents(vertex_id, graph).size();
            children[vertex_id] = status.children(vertex_id, graph).size();
        }

        bool is_correct = true;
        auto weights = boost::get(vertex_weight_t(), graph);
        std::unordered_set<size_t> distinct_vertices;
        size_t job_pos(0);
        for (auto &job : schedule)
        {
            bool step_correct = true;
            weight_t release = children[job.id] ? 0 : job.volume;
            distinct_vertices.insert(job.id);

            auto vert_parents = status.parents(job.id, graph);
            for (auto parent_id : vert_parents)
            {
                children[parent_id]--;
                if (!children[parent_id])
                {
                    for (size_t parent_pos(0); parent_pos < job_pos; parent_pos++)
                        if (schedule[parent_pos].id == parent_id)
                        {
                            release += schedule[parent_pos].volume;
                            break;
                        }
                }
            }

            auto vert_children = status.children(job.id, graph);
            for (auto child_id : vert_children)
                parents[child_id]--;

            step_correct = job.release == release && job.volume == weights[job.id] && !parents[job.id];
            is_correct &= step_correct;

            // logging (if provided)
            {
                if (logging_ && !step_correct)
                {
                    info_ += "Job " + std::to_string(job.id) + " on step " + std::to_string(job_pos) + ":\n";
                    if (parents[job.id])
                    {
                        info_ += "\tThe left part of the schedule must include all";
                        info_ += " the prececessors of the vertices but this isn't the case\n";
                    }

                    if (job.release != release)
                        info_ += "\tComputed release resources is " + std::to_string(release) + " but job.release is " + std::to_string(job.release) + "\n";
                    if (job.volume != weights[job.id])
                        info_ += "\tVertex weight is " + std::to_string(weights[job.id]) + " but job.volume is " + std::to_string(job.volume) + "\n";
                    info_ += "\n";
                }
            }
            job_pos++;
        }

        // logging (if provided)
        {
            if (logging_ && distinct_vertices.size() != boost::num_vertices(graph))
            {
                info_ += "Schedule doesn't contain next vertices:\n\t";
                for (auto vertex_id : boost::make_iterator_range(boost::vertices(graph)))
                    if (!distinct_vertices.count(vertex_id))
                        info_ += std::to_string(vertex_id) + " ";
                info_ += "\n";
            }

            if (logging_ && dump_path_.size() > 0)
            {
                std::ofstream file(dump_path_);
                file << info_;
                file.close();
            }
        }

        return is_correct;
    }

    bool ScheduleChecker::isCorrect(const Graph &graph, const nlohmann::ordered_json &schedule)
    {
        auto solution = Schedule::load(schedule);
        return isCorrect(graph, solution);
    }

    bool ScheduleChecker::isCorrect(const Graph &graph, const std::string &schedule_path)
    {
        auto solution = Schedule::load(schedule_path);
        return isCorrect(graph, solution);
    }
}