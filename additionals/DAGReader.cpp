#include "DAGReader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>
#include <cstring>

#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/graph_traits.hpp>

namespace scheduling_problem::additionals
{
    using scheduling_problem::edge_buffer_id_t;
    using scheduling_problem::edge_kind_t;
    using scheduling_problem::EdgeKind;
    using scheduling_problem::vertex_exec_time_t;
    using scheduling_problem::vertex_num_t;
    using scheduling_problem::vertex_weight_t;
    using scheduling_problem::weight_t;

namespace {
    /**
     * @brief Trim leading and trailing ASCII whitespace.
     * @param s Input string.
     * @return Trimmed view as a new string.
     */
    inline std::string trim(const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
        return s.substr(a, b - a);
    }

    /**
     * @brief Case-insensitive prefix check after trimming.
     * @param s      Input string.
     * @param prefix ASCII prefix to check.
     * @return True if @p s (trimmed) starts with @p prefix ignoring case.
     */
    inline bool starts_with_case(const std::string& s, const char* prefix)
    {
        std::string t = trim(s);
        for (size_t i = 0; prefix[i] && i < t.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(t[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        return std::strlen(prefix) <= t.size();
    }

    /**
     * @brief Parse all signed integers from a whitespace-separated fragment.
     * @param s Input text fragment.
     * @return Parsed integer list in encounter order.
     */
    inline std::vector<long long> parseIntegers(const std::string& s)
    {
        std::vector<long long> values;
        std::istringstream iss(s);
        long long value = 0;
        while (iss >> value) values.push_back(value);
        return values;
    }
} // anonymous namespace

    /**
     * @brief Construct a reader that scans a directory tree for graph files.
     *
     * Recursively collects file paths under @p directory, sorts them, and caps
     * the total sample count to the number of discovered paths if necessary.
     *
     * @param directory  Root directory to scan.
     * @param n_samples  Maximum number of graphs to read.
     * @param batch_size Number of graphs returned per nextBatch() call.
     */
    DAGReader::DAGReader(std::string directory,
                         unsigned n_samples,
                         unsigned batch_size)
        : DAGPool(n_samples, batch_size), directory_(std::move(directory))
    {
        std::filesystem::recursive_directory_iterator it(directory_), end;
        for (; it != end; ++it)
            if (it->is_regular_file() && it->path().extension() == ".txt")
                paths_.push_back(it->path());
        std::sort(paths_.begin(), paths_.end());
        if (paths_.size() < n_samples_ || !n_samples_) n_samples_ = paths_.size();
    }

    /**
     * @brief Produce the next batch of parsed graphs from discovered files.
     *
     * Iterates file list until either @p batch_size_ graphs are produced or
     * @p n_samples_ is exhausted. Only files with a detectable DAG format are parsed.
     *
     * @return Batch with parsed graphs and assigned batch id.
     */
    DAGPool::Batch DAGReader::nextBatch()
    {
        Batch graphs(batch_id_++);
        unsigned sample = 0;
        while (current_sample_ < paths_.size() && emitted_samples_ < n_samples_ && sample < batch_size_)
        {
            std::string filename = paths_[current_sample_].string();
            if (isDAG(filename))
            {
                graphs.push_back(make(filename));
                ++sample;
                ++emitted_samples_;
            }
            ++current_sample_;
        }
        return graphs;
    }

    /**
     * @brief Read a single graph from file if the format is supported.
     * @param filename Path to file.
     * @return Parsed graph or empty graph if format is not recognized.
     */
    Graph DAGReader::read(const std::string &filename)
    {
        if (isDAG(filename)) return make(filename);
        return Graph();
    }

    /**
     * @brief Detect whether a file appears to contain a supported DAG format.
     *
     * Recognizes:
     *  1) Legacy header line: "node size children".
     *  2) New format with header line starting with "prog_id" (case-insensitive).
     *  3) New format without header where first significant line begins with an integer id.
     *
     * @param filename Path to file.
     * @return True if the file is likely a supported DAG format.
     */
    bool DAGReader::isDAG(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file) return false;

        auto strip_bom = [](std::string& s) {
            if (s.size() >= 3 &&
                static_cast<unsigned char>(s[0]) == 0xEF &&
                static_cast<unsigned char>(s[1]) == 0xBB &&
                static_cast<unsigned char>(s[2]) == 0xBF) {
                s.erase(0, 3);
            }
        };

        std::string line;
        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty()) continue;
            strip_bom(line);

            {
                std::istringstream iss(line);
                std::string a, b, c;
                iss >> a >> b >> c;
                if (a == "node" && b == "size" && c == "children")
                    return true;
            }

            {
                std::string lower = line;
                for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (lower.rfind("prog_id", 0) == 0)
                    return true;
            }

            {
                std::istringstream iss(line);
                long long id;
                if (iss >> id) return true;
            }
        }

        return false;
    }

    /**
     * @brief Parse a graph by auto-detecting the input format.
     *
     * Peeks the first non-empty line to decide between legacy and new format,
     * rewinds the stream, then dispatches to the corresponding parser.
     *
     * @param filename Path to the input file.
     * @return Parsed graph or empty graph on failure.
     */
    Graph DAGReader::make(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file) return Graph();

        std::string first;
        while (std::getline(file, first))
        {
            first = trim(first);
            if (!first.empty()) break;
        }

        file.clear();
        file.seekg(0);

        {
            std::istringstream iss(first);
            std::string a,b,c;
            iss >> a >> b >> c;
            if (a == "node" && b == "size" && c == "children")
                return makeOld(file, filename);
        }

        return makeNew(file, filename);
    }

    /**
     * @brief Parse legacy format: header "node size children", then lines "id weight children...".
     *
     * Builds a normalized graph with contiguous vertex ids, assigns vertex weights,
     * and creates edges without buffers (buffer_id = 0, edge_weight equals parent weight).
     *
     * @param file      Open input stream positioned at beginning.
     * @param filename  Full path for naming purposes.
     * @return Parsed graph.
     */
    Graph DAGReader::makeOld(std::ifstream &file, const std::string &filename)
    {
        std::map<size_t, std::set<size_t>> adjacencies;
        std::map<size_t, weight_t> weights;
        std::set<long long> vertices;

        std::string header;
        std::getline(file, header);

        std::string line;
        while (std::getline(file, line))
        {
            if (line.size() <= 1) continue;
            std::istringstream iss(line);
            long long raw_node;
            weight_t w;
            iss >> raw_node >> w;

            size_t node = static_cast<size_t>(raw_node);
            weights[node] = w;

            std::set<size_t> children{ std::istream_iterator<size_t>(iss),
                                       std::istream_iterator<size_t>() };
            adjacencies[node] = std::move(children);
            vertices.insert(raw_node);
            for (auto ch : adjacencies[node]) vertices.insert(static_cast<long long>(ch));
        }

        auto vertex_map = cleanData(vertices);

        size_t n_vertex = vertices.size();
        auto fname = boost::replace_all_copy(filename, "\\", "/");
        size_t name_start = fname.rfind("/") + 1, name_end = fname.rfind(".");
        std::string graph_name = filename.substr(name_start, name_end - name_start);
        Graph graph(n_vertex, graph_name);

        auto vweights = boost::get(vertex_weight_t(), graph);
        auto vexec = boost::get(vertex_exec_time_t(), graph);
        auto vnum = boost::get(vertex_num_t(), graph);
        for (auto &kv : weights)
        {
            auto vid = vertex_map[ static_cast<long long>(kv.first) ];
            vweights[vid] = kv.second;
            vexec[vid] = kv.second;
            vnum[vid] = kv.first;
        }

        for (auto &kv : adjacencies)
        {
            size_t parent_norm = vertex_map[ static_cast<long long>(kv.first) ];
            for (auto child : kv.second)
            {
                size_t child_norm = vertex_map[ static_cast<long long>(child) ];
                auto [e, ok] = boost::add_edge(parent_norm, child_norm, graph);
                if (ok)
                {
                    boost::put(edge_buffer_id_t(), graph, e, 0);
                    boost::put(boost::edge_weight,  graph, e, weights[kv.first]);
                    boost::put(edge_kind_t(),       graph, e, EdgeKind::Real);
                }
            }
        }
        return graph;
    }

    /**
     * @brief Parse new format with optional header and buffer groups.
     *
     * Each non-empty line begins with a raw node id, optionally followed by
     * an execution time, and then one or more buffer groups separated by commas.
     * Each group is either
     * "weight: child1 child2 ..." or legacy-like "weight child1 child2 ...".
     * Vertex weights are the sum of group weights per vertex; each group defines
     * edges with the same buffer_id and edge_weight equal to the group's weight.
     * If execution time is absent, it falls back to the sum of group weights.
     *
     * @param file      Open input stream positioned at beginning.
     * @param filename  Full path for naming purposes.
     * @return Parsed graph.
     */
    Graph DAGReader::makeNew(std::ifstream &file, const std::string &filename)
    {
        struct BufferGroup {
            weight_t w{};
            std::vector<long long> children;
        };
        struct NodeInfo {
            weight_t exec_time = 0;
            bool has_exec_time = false;
            std::vector<BufferGroup> groups;
        };
        std::unordered_map<long long, NodeInfo> per_node;
        std::set<long long> vertices;
        bool header_declares_exec_time = false;

        std::string line;
        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty()) continue;

            if (starts_with_case(line, "prog_id"))
            {
                std::string lower = line;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                header_declares_exec_time = lower.find("prog_time") != std::string::npos;
                continue;
            }

            std::istringstream iss(line);

            long long raw_node;
            if (!(iss >> raw_node)) continue;

            vertices.insert(raw_node);

            std::string rest;
            std::getline(iss, rest);
            rest = trim(rest);
            if (rest.empty())
            {
                continue;
            }

            std::vector<std::string> groups;
            {
                std::string cur;
                std::stringstream ss(rest);
                while (std::getline(ss, cur, ','))
                {
                    cur = trim(cur);
                    if (!cur.empty()) groups.push_back(cur);
                }
            }

            auto &node_info = per_node[raw_node];
            bool first_group = true;
            for (auto &g : groups)
            {
                if (g.empty()) continue;

                auto colon = g.find(':');
                if (colon == std::string::npos)
                {
                    const auto nums = parseIntegers(g);
                    if (nums.empty()) continue;

                    size_t cursor = 0;
                    if (first_group && header_declares_exec_time && nums.size() >= 2 && !node_info.has_exec_time)
                    {
                        node_info.exec_time = static_cast<weight_t>(nums[cursor++]);
                        node_info.has_exec_time = true;
                    }

                    if (cursor >= nums.size()) continue;

                    BufferGroup bg;
                    bg.w = static_cast<weight_t>(nums[cursor++]);
                    for (; cursor < nums.size(); ++cursor)
                    {
                        bg.children.push_back(nums[cursor]);
                        vertices.insert(nums[cursor]);
                    }

                    node_info.groups.push_back(std::move(bg));
                    first_group = false;
                    continue;
                }

                weight_t w = 0;
                const auto lhs = parseIntegers(trim(g.substr(0, colon)));
                if (lhs.empty()) continue;
                if (first_group && lhs.size() >= 2 && !node_info.has_exec_time)
                {
                    node_info.exec_time = static_cast<weight_t>(lhs.front());
                    node_info.has_exec_time = true;
                    w = static_cast<weight_t>(lhs[1]);
                }
                else
                {
                    w = static_cast<weight_t>(lhs.front());
                }

                std::string rhs = trim(g.substr(colon + 1));
                std::istringstream cr(rhs);

                BufferGroup bg; bg.w = w;
                long long child;
                while (cr >> child) { bg.children.push_back(child); vertices.insert(child); }

                node_info.groups.push_back(std::move(bg));
                first_group = false;
            }
        }

        auto vertex_map = cleanData(vertices);
        size_t n_vertex = vertices.size();

        auto fname = boost::replace_all_copy(filename, "\\", "/");
        size_t name_start = fname.rfind("/") + 1, name_end = fname.rfind(".");
        std::string graph_name = filename.substr(name_start, name_end - name_start);

        Graph graph(n_vertex, graph_name);

        auto vweights = boost::get(vertex_weight_t(), graph);
        auto vexec = boost::get(vertex_exec_time_t(), graph);
        auto vnum = boost::get(vertex_num_t(), graph);
        for (auto &kv : per_node)
        {
            long long raw = kv.first;
            size_t v = vertex_map[raw];
            weight_t sum = 0;
            for (auto &bg : kv.second.groups) sum += bg.w;
            vweights[v] = sum;
            vexec[v] = kv.second.has_exec_time ? kv.second.exec_time : sum;
            vnum[v] = static_cast<size_t>(raw);
        }

        for (auto &kv : per_node)
        {
            size_t parent = vertex_map[kv.first];
            int buffer_id = 0;
            for (auto &bg : kv.second.groups)
            {
                for (auto child_raw : bg.children)
                {
                    auto it = vertex_map.find(child_raw);
                    if (it == vertex_map.end()) continue;
                    size_t child = it->second;

                    auto [e, ok] = boost::add_edge(parent, child, graph);
                    if (!ok) continue;

                    boost::put(edge_buffer_id_t(), graph, e, buffer_id);
                    boost::put(boost::edge_weight,  graph, e, bg.w);
                    boost::put(edge_kind_t(),       graph, e, EdgeKind::Real);
                }
                ++buffer_id;
            }
        }

        return graph;
    }

    /**
     * @brief Map arbitrary vertex ids (possibly negative) to [0..N-1] in ascending order.
     * @param vertices Set of raw vertex ids.
     * @return Mapping raw_id -> normalized_id.
     */
    std::unordered_map<long long, size_t> DAGReader::cleanData(std::set<long long> &vertices)
    {
        std::unordered_map<long long, size_t> vertex_map;
        size_t mapped = 0;
        for (auto &vid : vertices)
            vertex_map[vid] = mapped++;
        return vertex_map;
    }
}
