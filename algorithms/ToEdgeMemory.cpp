#include "ToEdgeMemory.h"

#include <iostream>
#include <vector>

namespace scheduling_problem::algorithms
{
  Graph to_edge_memory(const Graph &graph)
  {
    Graph new_graph = graph;

    auto last_vertex = boost::make_iterator_range(boost::vertices(graph))[0];

    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
      if (!boost::out_degree(vertex, graph))
      {
        last_vertex = vertex;

        auto last_vertex_weight = boost::get(vertex_weight_t(), graph, last_vertex);

        // need to add node with 0 weight
        auto new_last_vertex = boost::add_vertex(new_graph);
        boost::put(vertex_weight_t(), new_graph, new_last_vertex, 0);

        auto new_edge = boost::add_edge(last_vertex, new_last_vertex, new_graph);

        last_vertex = new_last_vertex;

        break;
      }
    }

    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
      auto out_deg = boost::out_degree(vertex, graph);
      auto vertex_weight = boost::get(vertex_weight_t(), graph, vertex);

      // set current node weight to 0, because all weights are stored on edges
      boost::put(vertex_weight_t(), new_graph, vertex, 0);

      if (out_deg == 0)
      {
        // its a terminal node, and occupies resurses, we need to add another node to simulate resourse occupation
        continue;
      }
      else if (out_deg == 1)
      {
        //  its a node, which weight can be moved to outgoing edge
        auto out_edge = boost::make_iterator_range(boost::out_edges(vertex, new_graph))[0];

        boost::put(boost::edge_weight_t(), new_graph, out_edge, vertex_weight);
      }
      else if (out_deg >= 2 && vertex_weight)
      {
        // need ot add another vertex and make it adjacent to current node and all its sucessor
        auto new_vertex = boost::add_vertex(new_graph);

        for (auto edge = boost::out_edges(vertex, graph); edge.first != edge.second; ++edge.first)
        {
          auto succ_vertex = boost::target(*edge.first, graph);
          auto succ_new_edge = boost::add_edge(succ_vertex, new_vertex, new_graph);
          boost::put(boost::edge_weight_t(), new_graph, succ_new_edge.first, 0);
        }

        auto cur_new_edge = boost::add_edge(vertex, new_vertex, new_graph);
        boost::put(boost::edge_weight_t(), new_graph, cur_new_edge.first, vertex_weight);

        auto new_last_edge = boost::add_edge(new_vertex, last_vertex, new_graph);
        boost::put(boost::edge_weight_t(), new_graph, new_last_edge.first, 0);
      }
    }

    return new_graph;
  }

  ScheduleStatus clean_edge_weight_schedule(const Graph &graph, const ScheduleStatus &schedule)
  {
    int num_vert = boost::num_vertices(graph);
    ScheduleStatus clean_schedule(graph);

    for (auto job : schedule)
    {
      if (job.id < num_vert)
      {
        clean_schedule.insert(job.id, clean_schedule.size(), graph);
      }
    }

    return clean_schedule;
  }
}