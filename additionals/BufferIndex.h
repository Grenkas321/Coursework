#pragma once
#include <unordered_map>
#include <vector>
#include "general_types.h"

namespace scheduling_problem::additionals {

/**
 * @brief Aggregated info for a group of edges sharing the same buffer id from a given parent.
 */
struct BufferGroupInfo {
    /**
     * @brief Buffer weight (identical for all edges in the group).
     */
    weight_t weight{};

    /**
     * @brief Normalized ids of consumer vertices (targets of the grouped edges).
     */
    std::vector<size_t> consumers;
};

/**
 * @brief Index to access buffer groups by parent vertex and buffer id.
 *
 * Conceptually: groups[parent][buffer_id] -> BufferGroupInfo.
 */
struct BufferIndex {
    /**
     * @brief Mapping parent -> (buffer_id -> group info).
     */
    std::vector<std::unordered_map<int, BufferGroupInfo>> groups;
};

/**
 * @brief Build a buffer index over all Real edges of the graph.
 *
 * Scans edges of @p g and groups them by (parent vertex, edge_buffer_id_t).
 * For each group, stores the buffer weight (from edge_weight) and the list of consumer vertices.
 * Non-Real edges are ignored.
 *
 * @param g  Input graph.
 * @return BufferIndex with per-parent, per-buffer groups populated.
 */
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
 * @brief Predicate: will vertex @p v become the last consumer of a buffer group.
 *
 * Determines if scheduling @p v would close the buffer group @p bg, assuming that
 * all already scheduled tasks are marked by scheduled[child] == true.
 *
 * @param v         Candidate consumer vertex id.
 * @param bg        Buffer group info (consumers of the same buffer from one parent).
 * @param scheduled Boolean flags for vertices that are already placed.
 * @return true if all other consumers in the group are already scheduled; false otherwise.
 */
inline bool willCloseBuffer(size_t v,
                            const BufferGroupInfo& bg,
                            const std::vector<char>& scheduled) {
    for (auto c : bg.consumers) {
        if (c == v) continue;
        if (!scheduled[c]) return false;
    }
    return true;
}

/**
 * @brief Fast estimate of immediate buffer release if @p v is placed next.
 *
 * Sums the buffer weights for all parent->v groups that would be closed by scheduling @p v,
 * given the current @p scheduled mask. Only considers Real edges and groups found in @p bi.
 *
 * @param g          Graph.
 * @param bi         Prebuilt buffer index (from buildBufferIndex).
 * @param v          Candidate vertex to place.
 * @param scheduled  Boolean flags for already scheduled vertices.
 * @return Total buffer weight that would be released immediately.
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

}
