#include "SeriesParallel.h"
#include "Greedy.h"
#include "ScheduleChecker.h"

namespace scheduling_problem::algorithms
{
    SeriesParallel::SeriesParallel(const std::string &label) : BaseOptimization(label) {}

    std::unique_ptr<BaseOptimization> SeriesParallel::copy() const
    {
        return std::unique_ptr<SeriesParallel>(new SeriesParallel(label_));
    }

    Schedule SeriesParallel::schedule_(const Graph &graph_)
    {
        auto graph = additionals::topologicalSort(graph_).first;
        auto save = graph;

        auto ret = find_sp_subgraph(graph);
        auto sp_subgraph = ret.first;
        scheduling_problem::Graph edge_mem_graph;

        bool graph_is_sp = boost::num_vertices(graph) == boost::num_vertices(sp_subgraph) && boost::num_edges(graph) == boost::num_edges(sp_subgraph);

        if (graph_is_sp)
        {
            // the input grpah is already series-parallel, so we need to schedule it in different greedy way
            // throw out all vertex weights for nodes with more than one out edge
            edge_mem_graph = sp_subgraph;

            int pred_w = 0;
            int post_w = 0;

            for (auto v : boost::make_iterator_range(boost::vertices(edge_mem_graph)))
            {
                auto w = boost::get(vertex_weight_t(), edge_mem_graph, v);
                pred_w += w;
                if (boost::out_degree(v, edge_mem_graph) == 1)
                {
                    post_w += w;
                    auto out = boost::make_iterator_range(boost::out_edges(v, edge_mem_graph))[0];
                    boost::put(boost::edge_weight_t(), edge_mem_graph, out, w);
                }
                else
                {
                    for (auto out : boost::make_iterator_range(boost::out_edges(v, edge_mem_graph)))
                    {
                        boost::put(boost::edge_weight_t(), edge_mem_graph, out, w);
                    }
                }

                boost::put(vertex_weight_t(), edge_mem_graph, v, 0);
            }
        }
        else
        {
            auto one_source_graph = add_source_target(graph);
            auto tmp = find_sp_subgraph(one_source_graph);

            edge_mem_graph = tmp.first;

            int pred_w = 0;
            int post_w = 0;

            for (auto v : boost::make_iterator_range(boost::vertices(edge_mem_graph)))
            {
                auto w = boost::get(vertex_weight_t(), edge_mem_graph, v);
                pred_w += w;
                if (boost::out_degree(v, edge_mem_graph) == 1)
                {
                    post_w += w;
                    auto out = boost::make_iterator_range(boost::out_edges(v, edge_mem_graph))[0];
                    boost::put(boost::edge_weight_t(), edge_mem_graph, out, w);
                }
                else
                {
                    for (auto out : boost::make_iterator_range(boost::out_edges(v, edge_mem_graph)))
                    {
                        boost::put(boost::edge_weight_t(), edge_mem_graph, out, w);
                    }
                }

                boost::put(vertex_weight_t(), edge_mem_graph, v, 0);
            }
        }

        auto cum_weight_graph = edge_to_vertex_mem(sp_subgraph);

        auto tmp2 = find_sp_subgraph(cum_weight_graph);
        auto sp_tree = tmp2.second;

        put_vertex_num(cum_weight_graph);

        auto cum_weight_schedule = schedule_sp_graph(cum_weight_graph, sp_tree);

        auto sp_edge_mem_schedule = clean_cum_weight_schedule(edge_mem_graph, cum_weight_schedule.first);

        auto subgraph_schedule = clean_edge_weight_schedule(graph, sp_edge_mem_schedule);

        scheduling_problem::ScheduleStatus edge_mem_schedule(graph);
        scheduling_problem::ScheduleStatus final_schedule(graph);

        if (graph_is_sp)
        {
            edge_mem_schedule = subgraph_schedule;
        }
        else
        {
            edge_mem_schedule = insert_vertices_and_edges(graph, sp_subgraph, subgraph_schedule);
        }

        final_schedule = clean_edge_weight_schedule(graph, edge_mem_schedule);

        additionals::ScheduleChecker checker;

        if (!checker.isCorrect(save, final_schedule))
        {
            throw std::exception();
        }

        return final_schedule;
    }
}
