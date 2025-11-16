#include "additionals.h"
#include <stack>
#include <numeric>
#include <fstream>
#include <iostream>

namespace scheduling_problem::additionals
{
    /**
     * @brief Print graph to stdout
     */
    void printNetwork(const scheduling_problem::Graph &graph)
    {
        auto vertices = boost::vertices(graph);
        std::cout << "======" << graph.name() << "======" << std::endl;
        for (auto vertex = vertices.first; vertex < vertices.second; vertex++)
        {
            std::cout << "Vertex: " << *vertex << std::endl;
            std::cout << "\tWeight: " << boost::get(scheduling_problem::vertex_weight_t(), graph, *vertex) << std::endl;
            auto out = boost::out_edges(*vertex, graph);
            std::for_each(out.first, out.second,
                          [&](auto edge)
                          {
                              auto target = boost::target(edge, graph);
                              std::cout << "\t" << *vertex << " ---> " << target << std::endl;
                              ;
                          });

            auto in = boost::in_edges(*vertex, graph);
            std::for_each(in.first, in.second,
                          [&](auto edge)
                          {
                              auto source = boost::source(edge, graph);
                              std::cout << "\t" << *vertex << " <--- " << source << std::endl;
                              ;
                          });
        }
    }

    /**
     * @brief Print schedule to stdout
     */
    void printSolution(scheduling_problem::Schedule &solution, bool full)
    {
        if (full)
        {
            for (size_t i = 0; i < solution.size(); i++)
            {
                std::cout << "Vetex: " << solution[i].id << "\n\tCost: " << solution[i].volume << std::endl;
            }
        }
        std::cout << "Solution cost: " << solution.cost() << std::endl;
    }

    /**
     * @brief Serialize graph to file
     *
     * File name will contain of
     * graph name and '.txt' suffix
     */
    void serializeGraph(const Graph &graph, std::string path)
    {
        std::ofstream file(path + "/" + graph.name() + ".txt");
        file << "node\tsize\tchildren\n";
        auto vertices = boost::vertices(graph);
        for (auto vertex = vertices.first; vertex < vertices.second; vertex++)
        {
            file << *vertex << "\t" << boost::get(scheduling_problem::vertex_weight_t(), graph, *vertex);
            auto children = boost::adjacent_vertices(*vertex, graph);
            for (auto child = children.first; child < children.second; child++)
            {
                file << "\t" << *child;
            }
            file << std::endl;
        }
        file.close();
    }

    /**
     * @brief Colors for deep search
     */
    enum class VertexColor
    {
        WHITE,
        GRAY,
        BLACK
    };

    /**
     * @brief Deep search
     *
     * @param graph Input graph
     * @param curr_vid %Node number in the graph
     * @param vcolor Node colors
     * @param vstack Sequence of nodes
     */
    void deepSearch(const Graph &graph,
                    size_t curr_vid,
                    std::vector<VertexColor> &vcolor,
                    std::stack<size_t> &vstack)
    {
        if (vcolor[curr_vid] == VertexColor::WHITE)
        {
            vcolor[curr_vid] = VertexColor::GRAY;
            for (const auto &child : boost::make_iterator_range(boost::adjacent_vertices(curr_vid, graph)))
                if (vcolor[child] != VertexColor::BLACK)
                    deepSearch(graph, child, vcolor, vstack);

            vcolor[curr_vid] = VertexColor::BLACK;
            vstack.push(curr_vid);
        }
    }

    /**
     * @brief Graph topological sorting
     */
    std::pair<Graph, std::map<size_t, size_t>> topologicalSort(const Graph &graph)
    {
        auto num_vertices = boost::num_vertices(graph);
        std::vector<VertexColor> vcolor(num_vertices, VertexColor::WHITE);
        std::stack<size_t> vstack;
        for (const auto &curr_vid : boost::make_iterator_range(boost::vertices(graph)))
            if (!boost::in_degree(curr_vid, graph))
                deepSearch(graph, curr_vid, vcolor, vstack);

        std::map<size_t, size_t> vmapper;
        for (size_t vid(0); vid < num_vertices; vid++)
        {
            vmapper[vstack.top()] = vid;
            vstack.pop();
        }
        std::set<size_t> set;
        for (auto &pair : vmapper)
            set.insert(pair.second);

        Graph sorted_graph(num_vertices, graph.name());
        for (const auto &edge : boost::make_iterator_range(boost::edges(graph)))
            boost::add_edge(vmapper[edge.m_source], vmapper[edge.m_target], sorted_graph);

        auto weights = boost::get(vertex_weight_t(), graph);
        for (const auto &vid : boost::make_iterator_range(boost::vertices(graph)))
            boost::put(vertex_weight_t(), sorted_graph, vmapper[vid], weights[vid]);

        return {sorted_graph, vmapper};
    }

    /**
     * @brief Changes task numbers in schedule
     */
    Schedule reorderSchedule(std::map<size_t, size_t> &vmapper, const Schedule &schedule)
    {
        std::map<size_t, size_t> remap;
        for (auto &[vertex, mapped_vertex] : vmapper)
            remap[mapped_vertex] = vertex;
        Schedule reorder_schedule(schedule.size());
        for (auto &job : schedule)
        {
            reorder_schedule.push(remap[job.id], job.volume, job.release);
        }
        return reorder_schedule;
    }

}
