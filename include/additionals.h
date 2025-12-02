#pragma once

#include <map>
#include <string>
#include "general_types.h"
#include "Schedule.h"

namespace scheduling_problem::additionals
{
    /**
     * @param graph Input graph
     */
    void printNetwork(const Graph &graph);

    /**
     * @param solution Schedule instance
     * @param full Print schedule structure or not
     */
    void printSolution(Schedule &solution, bool full = true);

    /**
     * @param graph Input graph
     * @param path Directory for serialization
     */
    void serializeGraph(const Graph &graph, std::string path);

    /**
     * @param graph Input graph
     * @return Pair of elements <Graph (because function is recursive first parameter is used inside of it), topologically ordered nodes>
     */
    std::pair<Graph, std::map<size_t, size_t>> topologicalSort(const Graph &graph);

    /**
     * @param vmapper Node mapping
     * @param schedule %Schedule instance
     */
    Schedule reorderSchedule(std::map<size_t, size_t> &vmapper, const Schedule &schedule);
}
