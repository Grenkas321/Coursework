#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "BufferIndex.h"

namespace scheduling_problem::algorithms
{
    struct MemoryGroupState
    {
        weight_t weight = 0;
        size_t remaining = 0;
        weight_t parent_start = 0;
        weight_t parent_finish = 0;
        weight_t max_consumer_finish = 0;
        bool parent_scheduled = false;
    };

    struct MemoryEvent
    {
        weight_t time = 0;
        weight_t delta = 0;
    };

    struct MemoryBaseContext
    {
        weight_t produced_upper = 0;
        std::vector<MemoryEvent> events;
        std::vector<weight_t> change_times;
    };

    bool isUnlimitedMemory(weight_t limit);

    std::vector<std::unordered_map<int, MemoryGroupState>>
    makeMemoryGroups(const additionals::BufferIndex &bindex);

    std::vector<std::vector<std::pair<size_t, int>>>
    buildIncomingGroups(const Graph &graph);

    weight_t totalProducedWeight(const additionals::BufferIndex &bindex);

    MemoryBaseContext buildMemoryBaseContext(
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups);

    bool feasibleUnderMemory(
        weight_t memory_limit,
        const MemoryBaseContext &base,
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups,
        size_t candidate,
        weight_t cand_start,
        weight_t cand_finish,
        const std::vector<std::vector<std::pair<size_t, int>>> &incoming_groups);

    bool earliestFeasibleStart(
        weight_t dep_ready,
        weight_t duration,
        weight_t memory_limit,
        const MemoryBaseContext &base,
        const std::vector<std::unordered_map<int, MemoryGroupState>> &groups,
        size_t candidate,
        const std::vector<std::vector<std::pair<size_t, int>>> &incoming_groups,
        weight_t &start_out);
}
