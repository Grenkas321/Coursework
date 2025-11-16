#pragma once
#include <unordered_map>
#include <vector>
#include "general_types.h"

namespace scheduling_problem::additionals {

struct BufferGroupInfo {
    weight_t weight{};                 // вес буфера (одинаков для всех рёбер группы)
    std::vector<size_t> consumers;     // нормализованные id потребителей
};

struct BufferIndex {
    // parent -> buffer_id -> info
    std::vector<std::unordered_map<int, BufferGroupInfo>> groups;
};

inline BufferIndex buildBufferIndex(const Graph& g) {
    BufferIndex idx;
    idx.groups.resize(boost::num_vertices(g));
    for (auto e : boost::make_iterator_range(boost::edges(g))) {
        size_t parent = boost::source(e, g);
        size_t child  = boost::target(e, g);
        int bid       = boost::get(edge_buffer_id_t(), g, e);
        auto w        = boost::get(boost::edge_weight,  g, e);
        auto kind     = boost::get(edge_kind_t(),       g, e);
        if (kind != EdgeKind::Real) continue;

        auto& slot = idx.groups[parent][bid];
        if (slot.weight == 0) slot.weight = w;
        slot.consumers.push_back(child);
    }
    return idx;
}

/**
 * Предикат: станет ли задача v «последним потребителем» буфера (p,bid),
 * при условии, что все уже поставленные задачи отмечены scheduled[child]==true.
 */
inline bool willCloseBuffer(size_t v,
                            const BufferGroupInfo& bg,
                            const std::vector<char>& scheduled) {
    for (auto c : bg.consumers) {
        if (c == v) continue;
        if (!scheduled[c]) return false;
    }
    return true; // v — последний потребитель
}

/**
 * Быстрая оценка «мгновенного релиза» при постановке v (считая уже поставленные).
 */
inline weight_t projectedReleaseIfPlace(const Graph& g,
                                        const BufferIndex& bi,
                                        size_t v,
                                        const std::vector<char>& scheduled) {
    weight_t rel = 0;
    for (auto parent : boost::make_iterator_range(boost::inv_adjacent_vertices(v, g))) {
        auto pr = boost::edge(parent, v, g);
        if (!pr.second) continue;
        auto e   = pr.first;
        int bid  = boost::get(edge_buffer_id_t(), g, e);
        auto itp = bi.groups[parent].find(bid);
        if (itp == bi.groups[parent].end()) continue;
        if (willCloseBuffer(v, itp->second, scheduled))
            rel += itp->second.weight;
    }
    return rel;
}

} // namespace
