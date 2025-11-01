#include "DAGReader.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/replace.hpp>

namespace scheduling_problem::additionals
{
    DAGReader::DAGReader(std::string directory,
                         unsigned n_samples,
                         unsigned batch_size)
        : DAGPool(n_samples, batch_size), directory_(directory)
    {
        std::filesystem::recursive_directory_iterator current(directory_), end;
        for (; current != end; current++)
        {
            paths_.push_back(current->path());
        }
        std::sort(paths_.begin(), paths_.end());
        if (paths_.size() < n_samples_ || !n_samples_)
            n_samples_ = paths_.size();
    }

    DAGPool::Batch DAGReader::nextBatch()
    {
        Batch graphs(batch_id_++);
        unsigned sample(0);
        while (current_sample_ < n_samples_ && sample < batch_size_)
        {
            if (paths_[current_sample_].extension() == ".txt")
            {
                std::string filename = paths_[current_sample_].string();
                if (isDAG(filename))
                    graphs.push_back(make(filename));
            }
            current_sample_++;
            sample++;
        }
        return graphs;
    }

    Graph DAGReader::read(const std::string &filename)
    {
        if (isDAG(filename))
            return make(filename);
        return Graph();
    }

    bool DAGReader::isDAG(const std::string &filename)
    {
        std::string node, size, children;
        std::ifstream file(filename);
        file >> node >> size >> children;
        file.close();
        return node == "node" && size == "size" && children == "children";
    }

    Graph DAGReader::make(const std::string &filename)
    {
        std::map<size_t, std::set<size_t>> adjacencies;
        std::map<size_t, weight_t> weights;
        std::set<size_t> vertices; // for vertex_id mapping

        std::ifstream file(filename);
        // skip header (node size children)
        std::string header;
        std::getline(file, header);

        // read from file
        while (!file.eof())
        {
            std::string str;
            std::getline(file, str);
            if (str.size() > 1)
            {
                std::istringstream iss(str);
                size_t node;
                iss >> node >> weights[node];

                // make adjacencies for node from remained numbers
                adjacencies[node] = {std::istream_iterator<size_t>(iss),
                                     std::istream_iterator<size_t>()};
                vertices.insert(node);
            }
        }
        file.close();
        // Huawei graphs have some vertex_id skips, so we should correct it
        auto vertex_map = cleanData(vertices);

        // construct graph
        size_t n_vertex = vertices.size();
        auto fname = boost::replace_all_copy(filename, "\\", "/");
        size_t name_start = fname.rfind("/") + 1, name_end = fname.rfind(".");
        std::string graph_name = filename.substr(name_start, name_end - name_start);
        Graph graph(n_vertex, graph_name);
        auto weight_map = boost::get(scheduling_problem::vertex_weight_t(), graph);
        for (auto &node_info : adjacencies)
        {
            auto node = node_info.first;
            for (auto &child : node_info.second)
                boost::add_edge(vertex_map[node], vertex_map[child], graph);
            weight_map[vertex_map[node]] = weights[node];
        }
        return graph;
    }

    std::unordered_map<size_t, size_t> DAGReader::cleanData(std::set<size_t> &vertices)
    {
        std::unordered_map<size_t, size_t> vertex_map;
        size_t mapped_id = 0;
        for (auto &vertex_id : vertices)
            vertex_map[vertex_id] = mapped_id++;
        return vertex_map;
    }
}
