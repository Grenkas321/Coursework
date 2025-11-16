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

    // Примитивный хешер для пары (parent_id, buffer_id)
    struct PairHash {
        size_t operator()(const std::pair<size_t,int>& p) const noexcept {
            // смешаем id вершины и buffer_id
            return std::hash<size_t>{}(p.first) ^ (static_cast<size_t>(p.second) + 0x9e3779b97f4a7c15ULL + (p.first<<6) + (p.first>>2));
        }
    };

    static bool check_by_buffers_impl(const Graph& graph,
                                      const Schedule& sched,
                                      std::string* msg,
                                      weight_t* out_peak = nullptr)
    {
        // remain[(parent, buffer_id)] = число оставшихся потребителей этого буфера
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

            // 1) Рост: выполнение продюсера создаёт ВСЕ его буферы
            target += job.volume;
            if (target > peak) peak = target;

            // 2) Релизы: закрываем те буферы, для которых это — последний потребитель
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
                        // последний потребитель — освобождаем РОВНО вес буфера
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

            // 3) Сверка с записанным release в расписании
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

            // 4) Инварианты: веса буферов и релизов (строгие проверки по желанию)
            if (job.id != static_cast<size_t>(-10000) && job.id != static_cast<size_t>(10000)) {
                // в задаче сказано: буфер не может иметь вес 0 (кроме первой/последней)
                // здесь это косвенно: если у вершины есть буферы, их веса > 0
                // Явно не проверяем, т.к. нулевые группы просто не влияют на релиз.
            }
        }

        if (out_peak) *out_peak = peak;
        return true;
    }

    bool ScheduleChecker::isCorrect(const Graph& graph,
                                    const Schedule& sched,
                                    std::string* message)
    {
        weight_t dummy_peak = 0;
        return check_by_buffers_impl(graph, sched, message, &dummy_peak);
    }

    weight_t ScheduleChecker::recomputePeak(const Graph& graph, const Schedule& sched)
    {
        weight_t peak = 0;
        std::string ignored;
        check_by_buffers_impl(graph, sched, &ignored, &peak);
        return peak;
    }
}
