#include "ToEdgeMemory.h"

#include <iostream>
#include <vector>

namespace scheduling_problem::algorithms
{
  /**
 * @brief Convert a vertex-weighted DAG to an edge-memory representation.
 *
 * Moves each vertex's processing time onto outgoing edges so that all
 * vertex weights become zero. Special handling:
 *  - If a sink is found, a dummy terminal vertex with zero weight is added
 *    to model resource occupation past the last real operation.
 *  - For out-degree == 1, the whole vertex weight is assigned to that single
 *    outgoing edge.
 *  - For out-degree >= 2 and a positive vertex weight, an auxiliary vertex is
 *    introduced and connected so that the weight is carried on a dedicated edge,
 *    while successors are re-wired through the auxiliary structure.
 *
 * The resulting graph may contain extra auxiliary vertices/edges, but all
 * original vertices have zero weight; edge weights encode execution costs.
 *
 * @param graph Input DAG whose vertex weights (vertex_weight_t) encode durations.
 * @return A new graph with zero vertex weights and costs transferred to edges
 *         (edge_weight_t), possibly with added auxiliary vertices/edges.
 */
  Graph to_edge_memory(const Graph &graph)
  {
    Graph new_graph = graph;

    auto last_vertex = boost::make_iterator_range(boost::vertices(graph))[0];

    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
      if (!boost::out_degree(vertex, graph))
      {
        last_vertex = vertex;
        
        auto new_last_vertex = boost::add_vertex(new_graph);
        boost::put(vertex_weight_t(), new_graph, new_last_vertex, 0);

        last_vertex = new_last_vertex;

        break;
      }
    }

    for (auto vertex : boost::make_iterator_range(boost::vertices(graph)))
    {
      auto out_deg = boost::out_degree(vertex, graph);
      auto vertex_weight = boost::get(vertex_weight_t(), graph, vertex);

      boost::put(vertex_weight_t(), new_graph, vertex, 0);

      if (out_deg == 0)
      {
        continue;
      }
      else if (out_deg == 1)
      {
        auto out_edge = boost::make_iterator_range(boost::out_edges(vertex, new_graph))[0];

        boost::put(boost::edge_weight_t(), new_graph, out_edge, vertex_weight);
      }
      else if (out_deg >= 2 && vertex_weight)
      {
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

  /**
 * @brief Project a schedule from an augmented edge-weight graph back to the original graph.
 *
 * Filters out auxiliary jobs introduced during edge-memory conversion by keeping
 * only jobs whose ids are strictly less than boost::num_vertices(graph), preserving
 * their relative execution order.
 *
 * @param graph     The original (pre-augmentation) graph; its vertex count bounds valid ids.
 * @param schedule  Schedule over the augmented graph (may include auxiliary jobs).
 * @return A schedule containing only the original jobs in their scheduled order.
 */
  ScheduleStatus clean_edge_weight_schedule(const Graph &graph, const ScheduleStatus &schedule)
  {
    size_t num_vert = boost::num_vertices(graph);
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
