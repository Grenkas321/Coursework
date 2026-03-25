#pragma once

#include <vector>
#include <string>
#include <map>
#include "general_types.h"
#include "json.hpp"

namespace scheduling_problem
{
    /**
     * Represents a single scheduled task (job).
     *
     * Each job is identified by its graph vertex id and carries:
     * - volume: the amount of resource produced/held at scheduling,
     * - release: the amount of resource released upon completion at its position.
     */
    struct Job
    {
        /** Graph vertex id of the job. */
        size_t id;
        /** Resource amount added/held when the job is scheduled. */
        weight_t volume;
        /** Resource amount released after the job completes at its position. */
        weight_t release;

        /**
         * Default constructor.
         */
        Job() = default;

        /**
         * Construct a job from components.
         *
         * @param id       Vertex id in the graph.
         * @param volume   Resource amount produced/held by the job.
         * @param release  Resource amount released on completion.
         */
        Job(size_t id,
            weight_t volume,
            weight_t release)
            : id(id), volume(volume), release(release)
        {
        }
    };

    /**
     * Processor-time placement metadata for a job in multiprocessor schedule.
     */
    struct JobPlacement
    {
        /** Processor index where the job executes. */
        unsigned processor = 0;
        /** Job start time. */
        weight_t start = 0;
        /** Job finish time. */
        weight_t finish = 0;
    };

    /**
     * Sequence of jobs forming a (partial or complete) schedule.
     *
     * The schedule tracks:
     * - cost_:   peak resource usage seen so far (objective),
     * - target_: current resource usage while replaying the sequence,
     * - name_:   human-readable identifier (typically the DAG name).
     *
     * Jobs are stored in execution order; use operator[] to access by position.
     * Supports JSON serialization/deserialization helpers.
     */
    class Schedule : public std::vector<Job>
    {
    protected:
        /** Peak usage (objective) and current usage while replaying. */
        weight_t cost_, target_;
        /** Wall-clock runtime of the algorithm that produced this schedule, in microseconds. */
        long long runtime_us_ = 0;
        /** Schedule name (used in JSON I/O). */
        std::string name_;
        /** Optional per-job placement metadata for multiprocessor schedules. */
        std::map<size_t, JobPlacement> placement_;
        /** Optional mapping from internal vertex ids to original graph ids. */
        std::map<size_t, size_t> display_ids_;

    public:
        /**
         * Construct an empty schedule with reserved capacity and a name.
         *
         * @param size  Expected number of jobs; used to reserve capacity.
         * @param name  Schedule identifier (default: "dag").
         */
        Schedule(size_t size = 0, const std::string &name = "dag");

        /**
         * Copy constructor: copies jobs and recomputes invariants on push.
         *
         * @param other  Schedule to copy from.
         */
        Schedule(const Schedule &other);

        /**
         * Move constructor: moves jobs and meta fields.
         *
         * @param other  Schedule to move from.
         */
        Schedule(Schedule &&other) noexcept;

        /**
         * Compute or return cost using cached per-job volumes/releases.
         *
         * @param recompute  If true, recompute cost_ and target_ from sequence.
         * @return Peak usage (schedule cost).
         */
        weight_t cost(bool recompute);

        /**
         * Recompute cost using graph semantics (e.g., Real edges and buffers).
         *
         * @param graph  Input task graph.
         * @return Peak usage (schedule cost).
         */
        weight_t cost(const Graph &graph);

        /**
         * Get the last computed cost.
         *
         * @return Cached peak usage.
         */
        weight_t cost() const;

        /**
         * Set cached objective/cost explicitly.
         *
         * @param value  Objective value to store.
         */
        void setCost(weight_t value);

        /**
         * Set runtime of the algorithm that produced this schedule.
         *
         * @param value  Runtime in microseconds.
         */
        void setRuntimeUs(long long value);

        /**
         * Get stored runtime of the algorithm that produced this schedule.
         *
         * @return Runtime in microseconds.
         */
        long long runtimeUs() const;

        /**
         * Set placement metadata for a specific job.
         *
         * @param id         Job id (vertex id).
         * @param processor  Processor index.
         * @param start      Start time.
         * @param finish     Finish time.
         */
        void setPlacement(size_t id, unsigned processor, weight_t start, weight_t finish);

        /**
         * Get all stored placement metadata.
         *
         * @return Map: job id -> placement.
         */
        const std::map<size_t, JobPlacement> &placement() const;

        /**
         * Remove all placement metadata.
         */
        void clearPlacement();

        /**
         * Override the user-facing id used during JSON serialization.
         *
         * @param id          Internal vertex id.
         * @param display_id  Original graph id to print in JSON.
         */
        void setDisplayId(size_t id, size_t display_id);

        /**
         * Append a job by components and update invariants.
         *
         * @param id       Graph vertex id.
         * @param volume   Resource amount produced/held by the job.
         * @param release  Resource amount released on completion.
         */
        void push(size_t id, weight_t volume, weight_t release);

        /**
         * Append a job looked up from the graph.
         * Uses vertex weight as volume; release is volume if out-degree is zero, else 0.
         *
         * @param graph  Input task graph.
         * @param id     Graph vertex id.
         */
        void push(const Graph &graph, size_t id);

        /**
         * Append a ready-made job and update invariants.
         *
         * @param job  Job to append.
         */
        void push(const Job &job);

        /**
         * Serialize the schedule to an ordered JSON object.
         *
         * @return JSON object with cost, size, and job map.
         */
        nlohmann::ordered_json json() const;

        /**
         * Write the JSON representation to a file.
         *
         * @param filename  Destination file path.
         */
        void dump(std::string filename) const;

        /**
         * Get the schedule name.
         *
         * @return Name string.
         */
        std::string name() const;

        /**
         * Copy assignment: rebuilds invariants via push().
         *
         * @param other  Source schedule.
         * @return *this
         */
        Schedule& operator=(const Schedule &other);

        /**
         * Move assignment.
         *
         * @param other  Source schedule.
         * @return *this
         */
        Schedule& operator=(Schedule &&other) noexcept;

        /**
         * Load a schedule from a JSON file produced by json()/dump().
         *
         * @param filename  Path to the JSON file.
         * @return Loaded schedule.
         */
        static Schedule load(std::string filename);

        /**
         * Load a schedule from a JSON object compatible with json().
         *
         * @param j  JSON object.
         * @return Loaded schedule.
         */
        static Schedule load(nlohmann::ordered_json j);

        /**
         * Build a JSON document for API/GUI use.
         * When with_schedule is false, only metadata is included.
         *
         * @param with_schedule  Include full job sequence if true.
         * @return JSON representation of the schedule.
         */
        nlohmann::ordered_json jsonRepresentation(bool with_schedule) const;

        /**
         * Save the JSON representation returned by jsonRepresentation(true) to disk.
         *
         * @param file_name  Destination file path.
         */
        void save(const std::string &file_name) const;
    };
}
