#include "Schedule.h"
#include <fstream>
#include <algorithm>
#include <unordered_map>

namespace scheduling_problem
{
    /**
     * Construct an empty schedule with reserved capacity and a human-readable name.
     *
     * @param size  Expected number of jobs; used to reserve capacity.
     * @param name  Schedule identifier (also used as the JSON object key).
     */
    Schedule::Schedule(size_t size, const std::string &name)
        : std::vector<Job>(), cost_(0), target_(0), name_(name)
    {
        reserve(size);
    }

    /**
     * Copy constructor.
     * Rebuilds the internal invariants (cost_/target_) by pushing jobs in order.
     *
     * @param other  Schedule to copy.
     */
    Schedule::Schedule(const Schedule &other) : Schedule(other.size(), other.name_) {
        for (auto &job : other) push(job);
    }

    /**
     * Move constructor.
     * Moves meta fields and job storage from the source schedule.
     *
     * @param other  Schedule to move from.
     */
    Schedule::Schedule(Schedule &&other) noexcept : std::vector<Job>(other.size())
    {
        cost_ = other.cost_;
        target_ = other.target_;
        name_ = std::move(other.name_);
        std::move(other.begin(), other.end(), this->begin());
    }

    /**
     * Compute or return the peak buffer usage without using the graph structure.
     * When recompute == true, cost_ and target_ are recomputed by replaying
     * jobs' volumes and releases; otherwise the cached cost_ is returned.
     *
     * @param recompute  If true, recompute from the current sequence of jobs.
     * @return Peak buffer usage (the schedule cost).
     */
    weight_t Schedule::cost(bool recompute)
    {
        if (recompute)
        {
            target_ = cost_ = 0;
            for (auto &job : *this)
            {
                target_ += job.volume;
                if (target_ > cost_) cost_ = target_;
                target_ -= job.release;
            }
        }
        return cost_;
    }

    /**
     * Recompute the schedule cost using the dependency graph.
     * Only edges with EdgeKind::Real are considered. A buffer associated with
     * a pair (parent vertex, buffer id) is released exactly when all of its
     * consumers have been scheduled; until then it remains occupied.
     *
     * Algorithm outline:
     * 1) Count, for each (parent, buffer_id), the number of remaining consumers
     *    over Real edges.
     * 2) Iterate jobs in schedule order:
     *    - Add job.volume to the current target_.
     *    - For each Real incoming edge from 'parent' with buffer id 'bid',
     *      decrement the remaining count for (parent, bid). When it reaches 0,
     *      release the corresponding buffer weight once.
     *    - Update job.release and subtract the accumulated release from target_.
     * 3) Track the maximum target_ reached as cost_.
     *
     * @param graph  Task graph with edge kinds, edge weights and buffer ids.
     * @return Peak buffer usage (the schedule cost).
     */
    weight_t Schedule::cost(const Graph &graph)
    {
        target_ = cost_ = 0;

        struct PairHash {
            size_t operator()(const std::pair<size_t,int>& p) const noexcept {
                return std::hash<size_t>{}(p.first) ^ (static_cast<size_t>(p.second) * 0x9e3779b97f4a7c15ULL);
            }
        };
        std::unordered_map<std::pair<size_t,int>, size_t, PairHash> remain;

        for (auto e : boost::make_iterator_range(boost::edges(graph))) {
            if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real) continue;
            auto parent = static_cast<size_t>(boost::source(e, graph));
            int bid     = boost::get(edge_buffer_id_t(), graph, e);
            ++remain[{parent, bid}];
        }

        for (auto &job : *this) {
            target_ += job.volume;
            if (target_ > cost_) cost_ = target_;

            weight_t release = 0;

            for (auto parent : boost::make_iterator_range(boost::inv_adjacent_vertices(job.id, graph))) {
                auto [e, ok] = boost::edge(parent, job.id, graph);
                if (!ok) continue;
                if (boost::get(edge_kind_t(), graph, e) != EdgeKind::Real) continue;

                int bid = boost::get(edge_buffer_id_t(), graph, e);
                auto key = std::make_pair(static_cast<size_t>(parent), bid);
                auto it = remain.find(key);
                if (it != remain.end() && it->second > 0 && --(it->second) == 0) {
                    release += boost::get(boost::edge_weight, graph, e);
                }
            }

            job.release = release;
            target_    -= release;
        }
        return cost_;
    }

    /**
     * Get the cached cost computed by a previous call to cost().
     *
     * @return Last computed peak buffer usage.
     */
    weight_t Schedule::cost() const { return cost_; }

    /**
     * Append a job by its components.
     * Updates internal invariants (cost_/target_) as if the job were executed next.
     *
     * @param id       Job identifier (vertex id in the graph).
     * @param volume   Produced volume added to the buffer at job start.
     * @param release  Amount released after scheduling this job (precomputed or 0).
     */
    void Schedule::push(size_t id, weight_t volume, weight_t release) { push(Job(id, volume, release)); }

    /**
     * Append a job by looking it up in the graph.
     * Uses vertex weight as the job volume. If the vertex has no outgoing edges,
     * the immediate release equals its volume; otherwise release is 0.
     *
     * @param graph  Task graph to query vertex weight and degree.
     * @param id     Vertex id of the job to append.
     */
    void Schedule::push(const Graph &graph, size_t id)
    {
        auto w = boost::get(vertex_weight_t(), graph, id);
        auto rel = (boost::out_degree(id, graph) == 0) ? w : 0;
        push(Job(id, w, rel));
    }

    /**
     * Append a ready-made Job and update internal invariants.
     *
     * @param job  Job to append to the end of the schedule.
     */
    void Schedule::push(const Job &job)
    {
        push_back(job);
        target_ += job.volume;
        if (target_ > cost_) cost_ = target_;
        target_ -= job.release;
    }

    /**
     * Serialize the schedule into an ordered JSON object.
     * Structure:
     * {
     *   "<name>": {
     *     "cost": <number>,
     *     "size": <number>,
     *     "schedule": { "<job_id>": [volume, release], ... }
     *   }
     * }
     *
     * @return Ordered JSON with schedule data.
     */
    nlohmann::ordered_json Schedule::json() const
    {
        nlohmann::ordered_json j;
        j[name_]["cost"] = cost_;
        j[name_]["size"] = size();
        for (auto &job : *this)
            j[name_]["schedule"][std::to_string(job.id)] = {job.volume, job.release};
        return j;
    }

    /**
     * Write the JSON representation of this schedule to a file.
     *
     * @param filename  Destination file path.
     */
    void Schedule::dump(std::string filename) const
    {
        std::ofstream file(filename);
        file << json().dump();
    }

    /**
     * Get the schedule name.
     *
     * @return Name string.
     */
    std::string Schedule::name() const { return name_; }

    /**
     * Copy assignment.
     * Clears current content and rebuilds from 'other' via push() to keep invariants.
     *
     * @param other  Source schedule.
     * @return *this
     */
    scheduling_problem::Schedule&
    scheduling_problem::Schedule::operator=(const Schedule &other)
    {
        if (this != &other) {
            this->clear();
            cost_   = 0;
            target_ = 0;
            name_   = other.name_;
            for (const auto &job : other) {
                this->push(job);
            }
        }
        return *this;
    }

    /**
     * Move assignment.
     * Moves the underlying vector and meta fields.
     *
     * @param other  Schedule to move from.
     * @return *this
     */
    scheduling_problem::Schedule&
    scheduling_problem::Schedule::operator=(Schedule &&other) noexcept
    {
        if (this != &other) {
            std::vector<Job>::operator=(std::move(other));
            cost_   = other.cost_;
            target_ = other.target_;
            name_   = std::move(other.name_);
        }
        return *this;
    }

    /**
     * Load a schedule from a JSON file produced by json()/dump().
     *
     * @param filename  Path to the JSON file.
     * @return Loaded Schedule instance.
     */
    Schedule Schedule::load(std::string filename)
    {
        std::ifstream file(filename);
        nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
        return load(j);
    }

    /**
     * Load a schedule from an ordered JSON object.
     * Expects the same structure produced by json().
     *
     * @param j  Ordered JSON with schedule data.
     * @return Loaded Schedule instance.
     */
    Schedule Schedule::load(nlohmann::ordered_json j)
    {
        std::string name = j.items().begin().key().c_str();
        size_t sz = j[name]["size"];
        Schedule s(sz, name);
        for (auto &elem : j[name]["schedule"].items())
        {
            size_t job_id;
            std::stringstream ss(elem.key());
            ss >> job_id;
            weight_t volume = elem.value()[0];
            weight_t release = elem.value()[1];
            s.push(job_id, volume, release);
        }
        return s;
    }
}
