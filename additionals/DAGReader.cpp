#include "DAGReader.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>
#include <cstring>   // <-- добавили

#include <boost/algorithm/string/replace.hpp>
#include <boost/graph/graph_traits.hpp>
#include <iostream>

namespace scheduling_problem::additionals
{
    using scheduling_problem::edge_buffer_id_t;
    using scheduling_problem::edge_kind_t;
    using scheduling_problem::EdgeKind;
    using scheduling_problem::vertex_weight_t;
    using scheduling_problem::weight_t;

// ===== локальные хелперы только для этого .cpp =====
namespace {
    inline std::string trim(const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
        return s.substr(a, b - a);
    }

    inline bool starts_with_case(const std::string& s, const char* prefix)
    {
        std::string t = trim(s);
        for (size_t i = 0; prefix[i] && i < t.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(t[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        return std::strlen(prefix) <= t.size();
    }
} // anonymous namespace

    // ===== ctor / batching =====

    DAGReader::DAGReader(std::string directory,
                         unsigned n_samples,
                         unsigned batch_size)
        : DAGPool(n_samples, batch_size), directory_(std::move(directory))
    {
        std::filesystem::recursive_directory_iterator it(directory_), end;
        for (; it != end; ++it) paths_.push_back(it->path());
        std::sort(paths_.begin(), paths_.end());
        if (paths_.size() < n_samples_ || !n_samples_) n_samples_ = paths_.size();
    }

    DAGPool::Batch DAGReader::nextBatch()
    {
        std::cout << '{' << batch_id_ << '}' << std::endl;
        Batch graphs(batch_id_++);
        unsigned sample = 0;
        std::cout << current_sample_ << ' ' << n_samples_ << "   " << sample << ' ' << batch_size_ << std::endl;
        while (current_sample_ < n_samples_ && sample < batch_size_)
        {
            std::cout << paths_[current_sample_] << std::endl;
            if (paths_[current_sample_].extension() == ".txt")
            {
                std::string filename = paths_[current_sample_].string();
                if (isDAG(filename))
                    graphs.push_back(make(filename));
            }
            ++current_sample_;
            ++sample;
        }
        std::cout << graphs << std::endl;
        return graphs;
    }

    Graph DAGReader::read(const std::string &filename)
    {
        if (isDAG(filename)) return make(filename);
        return Graph();
    }

    // ===== format detection =====

    bool DAGReader::isDAG(const std::string &filename)
{
    std::ifstream file(filename);
    if (!file) return false;

    auto strip_bom = [](std::string& s) {
        // убираем UTF-8 BOM, если вдруг есть
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

        // 1) Старый формат: "node size children"
        {
            std::istringstream iss(line);
            std::string a, b, c;
            iss >> a >> b >> c;
            if (a == "node" && b == "size" && c == "children")
                return true;
        }

        // 2) Новый формат с заголовком: "prog_id ..."
        {
            // допускаем любые регистры, пробелы
            std::string lower = line;
            for (char& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (lower.rfind("prog_id", 0) == 0) // начинается с "prog_id"
                return true;
        }

        // 3) Новый формат без заголовка: первая «значимая» строка начинается с числа (в т.ч. отрицательного)
        {
            std::istringstream iss(line);
            long long id;
            if (iss >> id) return true;
        }

        // если это какая-то другая строка (комментарий и т.п.), читаем дальше
    }

    return false;
}


    // ===== make() dispatch =====

    Graph DAGReader::make(const std::string &filename)
    {
        std::ifstream file(filename);
        if (!file) return Graph();

        // Снимем первую непустую строку, чтобы определить формат
        std::streampos start_pos = file.tellg();

        std::string first;
        while (std::getline(file, first))
        {
            first = trim(first);
            if (!first.empty()) break;
        }

        // Вернёмся к началу для конкретного парсера
        file.clear();
        file.seekg(0);

        // Старый формат
        {
            std::istringstream iss(first);
            std::string a,b,c;
            iss >> a >> b >> c;
            if (a == "node" && b == "size" && c == "children")
                return makeOld(file, filename);
        }

        // Новый формат
        return makeNew(file, filename);
    }

    // ===== old format =====
    Graph DAGReader::makeOld(std::ifstream &file, const std::string &filename)
    {
        std::map<size_t, std::set<size_t>> adjacencies;
        std::map<size_t, weight_t> weights;
        std::set<long long> vertices; // поддержим произвольные id (на входе), ниже замапим

        // пропускаем заголовок
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

        // сконструируем граф
        size_t n_vertex = vertices.size();
        auto fname = boost::replace_all_copy(filename, "\\", "/");
        size_t name_start = fname.rfind("/") + 1, name_end = fname.rfind(".");
        std::string graph_name = filename.substr(name_start, name_end - name_start);
        Graph graph(n_vertex, graph_name);

        auto vweights = boost::get(vertex_weight_t(), graph);
        for (auto &kv : weights)
            vweights[ vertex_map[ static_cast<long long>(kv.first) ] ] = kv.second;

        for (auto &kv : adjacencies)
        {
            size_t parent_norm = vertex_map[ static_cast<long long>(kv.first) ];
            for (auto child : kv.second)
            {
                size_t child_norm = vertex_map[ static_cast<long long>(child) ];
                // старый формат: буферов нет — пометим buffer_id=0, edge_weight=вес вершины
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

    // ===== new format =====
    Graph DAGReader::makeNew(std::ifstream &file, const std::string &filename)
    {
        // Накапливаем: для каждой вершины — список буферных групп (weight, children[])
        struct BufferGroup {
            weight_t w{};
            std::vector<long long> children;
        };
        std::unordered_map<long long, std::vector<BufferGroup>> per_node;
        std::set<long long> vertices;

        std::string line;
        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty()) continue;

            std::istringstream iss(line);

            long long raw_node;
            if (!(iss >> raw_node)) continue;

            vertices.insert(raw_node);

            // остаток строки содержит группы "weight: c1 c2 ... , weight: c3 ..."
            std::string rest;
            std::getline(iss, rest);
            rest = trim(rest);
            if (rest.empty())
            {
                // допустим пустых потребителей — вершина без выходов
                continue;
            }

            // разбить по запятым
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

            int buf_id = 0;
            for (auto &g : groups)
            {
                if (g.empty()) continue;

                auto colon = g.find(':');
                if (colon == std::string::npos)
                {
                    // бэк-совместимость: "size children..." без ':'
                    std::istringstream gg(g);
                    weight_t w = 0; gg >> w;

                    BufferGroup bg; bg.w = w;
                    long long child;
                    while (gg >> child) { bg.children.push_back(child); vertices.insert(child); }

                    per_node[raw_node].push_back(std::move(bg));
                    ++buf_id;
                    continue;
                }

                // левая часть — вес буфера
                weight_t w = 0;
                {
                    auto lw = trim(g.substr(0, colon));
                    std::istringstream bw(lw);
                    bw >> w;
                }

                // правая часть — список детей
                std::string rhs = trim(g.substr(colon + 1));
                std::istringstream cr(rhs);

                BufferGroup bg; bg.w = w;
                long long child;
                while (cr >> child) { bg.children.push_back(child); vertices.insert(child); }

                per_node[raw_node].push_back(std::move(bg));
                ++buf_id;
            }
        }

        // сопоставление произвольных id (в т.ч. отрицательных) к 0..N-1
        auto vertex_map = cleanData(vertices);
        size_t n_vertex = vertices.size();

        // имя графа
        auto fname = boost::replace_all_copy(filename, "\\", "/");
        size_t name_start = fname.rfind("/") + 1, name_end = fname.rfind(".");
        std::string graph_name = filename.substr(name_start, name_end - name_start);

        Graph graph(n_vertex, graph_name);

        // 1) веса вершин = сумма весов буферов
        auto vweights = boost::get(vertex_weight_t(), graph);
        for (auto &kv : per_node)
        {
            long long raw = kv.first;
            size_t v = vertex_map[raw];
            weight_t sum = 0;
            for (auto &bg : kv.second) sum += bg.w;
            vweights[v] = sum;
        }

        // 2) рёбра: для каждой группы буфера назначаем buffer_id и edge_weight = w
        for (auto &kv : per_node)
        {
            size_t parent = vertex_map[kv.first];
            int buffer_id = 0;
            for (auto &bg : kv.second)
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

    // ===== id compaction =====

    std::unordered_map<long long, size_t> DAGReader::cleanData(std::set<long long> &vertices)
    {
        std::unordered_map<long long, size_t> vertex_map;
        size_t mapped = 0;
        for (auto &vid : vertices)
            vertex_map[vid] = mapped++;
        return vertex_map;
    }
}
