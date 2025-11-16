#include "Schedule.h"
#include <fstream>
#include <algorithm>

namespace scheduling_problem
{
    /**
     * @brief Constructor
     */
    Schedule::Schedule(size_t size,
                       const std::string &name)
        : std::vector<Job>(), cost_(0), target_(0), name_(name)
    {
        reserve(size);
    }

    /**
     * @brief Constructor
     */
    Schedule::Schedule(const Schedule &other) : Schedule(other.size(), other.name_)
    {
        for (auto &job : other)
            push(job);
    }

    /**
     * @brief Constructor
     */
    Schedule::Schedule(Schedule &&other) noexcept : std::vector<Job>(other.size())
    {
        cost_ = other.cost_;
        target_ = other.target_;
        name_ = std::move(other.name_);
        std::move(other.begin(), other.end(), this->begin());
    }

    /**
     * @brief Get (and recalculate if requested) the goal function of the schedule
     *
     * Recalculates the value of the schedule goal function by using cached data
     * or returns pre-computed goal function value
     */
    weight_t Schedule::cost(bool recompute)
    {
        if (recompute)
        {
            target_ = cost_ = 0;
            for (auto &job : *this)
            {
                target_ += job.volume;
                if (target_ > cost_)
                    cost_ = target_;
                target_ -= job.release;
            }
        }
        return cost_;
    }

    /**
     * @brief Calculate the goal function of the schedule, assuming that the schedule was constructed for the given graph
     *
     * Recalculates the value of the schedule goal function on the given graph
     */
    weight_t Schedule::cost(const Graph &graph)
    {
    target_ = cost_ = 0;

    // Считаем для каждой группы (parent, buffer_id) сколько потребителей у этого буфера осталось
    struct PairHash {
        size_t operator()(const std::pair<size_t,int>& p) const noexcept {
            // простая смесь
            return std::hash<size_t>{}(p.first) ^ (static_cast<size_t>(p.second) + 0x9e3779b97f4a7c15ULL + (p.first<<6) + (p.first>>2));
        }
    };
    std::unordered_map<std::pair<size_t,int>, size_t, PairHash> remain;

    for (auto e : boost::make_iterator_range(boost::edges(graph))) {
        auto parent = boost::source(e, graph);
        int bid = boost::get(edge_buffer_id_t(), graph, e);
        remain[{static_cast<size_t>(parent), bid}]++;
    }

    // Проход по расписанию
    for (auto &job : *this)
    {
        // рост памяти — выполняем producer, создаём все его буферы
        target_ += job.volume;
        if (target_ > cost_) cost_ = target_;

        // релиз за счёт закрывающихся буферов (последние потребители)
        weight_t release = 0;
        for (auto parent : boost::make_iterator_range(boost::inv_adjacent_vertices(job.id, graph)))
        {
            auto pr = boost::edge(parent, job.id, graph);
            if (!pr.second) continue;
            auto e = pr.first;
            int bid = boost::get(edge_buffer_id_t(), graph, e);
            auto key = std::make_pair(static_cast<size_t>(parent), bid);
            auto it = remain.find(key);
            if (it != remain.end() && it->second > 0) {
                if (--(it->second) == 0) {
                    // освобождаем ровно вес буфера
                    release += boost::get(boost::edge_weight, graph, e);
                }
            }
        }
        job.release = release;
        target_ -= release;
    }
    return cost_;
    }


    /**
     * @brief Returns pre-calculated goal function
     */
    weight_t Schedule::cost() const
    {
        return cost_;
    }

    /**
     * @brief Adds a task to the end of the schedule and recomputes current goal function value
     */
    void Schedule::push(size_t id, weight_t volume, weight_t release)
    {
        push(Job(id, volume, release));
    }

    /**
     * @brief Adds a task to the end of the schedule and recomputes current goal function value
     */
    void Schedule::push(const Job &job)
    {
        push_back(job);

        target_ += job.volume;
        if (target_ > cost_)
            cost_ = target_;
        target_ -= job.release;
    }

    /**
     * @brief Construct json representation of %Schedule
     */
    nlohmann::ordered_json Schedule::json() const
    {
        nlohmann::ordered_json schedule;
        schedule[name_]["cost"] = cost_;
        schedule[name_]["size"] = size();
        for (auto &job : *this)
        {
            schedule[name_]["schedule"][std::to_string(job.id)] = {job.volume, job.release};
        }
        return schedule;
    }

    void Schedule::operator=(const Schedule &other)
    {
        clear();
        cost_ = 0;
        target_ = 0;
        name_ = other.name_;

        for (auto &job : other)
            push(job);
    }

    /**
     * @brief Dump schedule json representation to file
     */
    void Schedule::dump(std::string filename) const
    {
        std::ofstream file(filename);
        file << json().dump();
        file.close();
    }

    /**
     * @brief Get schedule name
     */
    std::string Schedule::name() const
    {
        return name_;
    }

    /**
     * @brief Load schedule from json file
     */
    Schedule Schedule::load(std::string filename)
    {
        std::ifstream file(filename);
        nlohmann::ordered_json j = nlohmann::ordered_json::parse(file);
        return load(j);
    }

    /**
     * @brief Load schedule from json object
     */
    Schedule Schedule::load(nlohmann::ordered_json j)
    {
        // first key in json is name of the graph (see dump)
        std::string name = j.items().begin().key().c_str();
        size_t size = j[name]["size"];
        Schedule schedule(size, name);

        // walk through all jobs
        for (auto &elem : j[name]["schedule"].items())
        {
            size_t job_id;
            std::stringstream sstream(elem.key());
            sstream >> job_id;

            weight_t volume, release;
            volume = elem.value()[0];
            release = elem.value()[1];
            schedule.push(job_id, volume, release);
        }

        return schedule;
    }
}
