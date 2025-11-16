#include "SeriesParallel.h"
#include "Greedy.h"
#include "ScheduleChecker.h"

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <climits>
#include "general_types.h"

#include <algorithm>
#include <memory>
#include <functional>
#include <queue>

#include <tuple>


using namespace scheduling_problem;

using namespace std;


vector<string> graphToContentFormatted(const auto& graph) {
    vector<string> content;
    
    // Структура: vertex -> vector of (buffer_id, weight, targets)
    map<Vertex, vector<tuple<int, weight_t, vector<Vertex>>>> vertexData;
    
    // Собираем информацию о ребрах с учетом buffer_id
    for (auto e : boost::make_iterator_range(boost::edges(graph))) {
        Vertex source = boost::source(e, graph);
        Vertex target = boost::target(e, graph);
        int buffer_id = boost::get(edge_buffer_id_t(), graph, e);
        weight_t weight = boost::get(boost::edge_weight, graph, e);
        
        // Находим или создаем запись для этого buffer_id
        auto& buffers = vertexData[source];
        auto it = find_if(buffers.begin(), buffers.end(), 
                         [buffer_id](const auto& tuple) { 
                             return get<0>(tuple) == buffer_id; 
                         });
        
        if (it != buffers.end()) {
            // Проверяем, что вес совпадает
            if (get<1>(*it) != weight) {
                cerr << "Warning: Different weights for same buffer_id " 
                     << buffer_id << " in vertex " << source << endl;
            }
            get<2>(*it).push_back(target);
        } else {
            buffers.push_back({buffer_id, weight, {target}});
        }
    }
    
    // Сортируем вершины
    vector<Vertex> vertices;
    auto vertex_range = boost::vertices(graph);
    for (auto it = vertex_range.first; it != vertex_range.second; ++it) {
        vertices.push_back(*it);
    }
    sort(vertices.begin(), vertices.end());
    
    // Создаем отформатированные строки
    for (Vertex v : vertices) {
        stringstream ss;
        
        // Номер вершины (выравнивание до 12 символов)
        ss << v;
        string vertexStr = ss.str();
        ss.str("");
        ss << vertexStr;
        for (int i = vertexStr.length(); i < 12; ++i) {
            ss << " ";
        }
        
        // Добавляем информацию о буферах
        if (vertexData.find(v) != vertexData.end()) {
            const auto& buffers = vertexData[v];
            
            // Сортируем буферы по buffer_id для единообразия
            vector<tuple<int, weight_t, vector<Vertex>>> sorted_buffers = buffers;
            sort(sorted_buffers.begin(), sorted_buffers.end(),
                [](const auto& a, const auto& b) {
                    return get<0>(a) < get<0>(b);
                });
            
            for (size_t i = 0; i < sorted_buffers.size(); ++i) {
                const auto& buffer = sorted_buffers[i];
                weight_t weight = get<1>(buffer);
                const auto& targets = get<2>(buffer);
                
                ss << weight << ":";
                
                // Сортируем потомков для единообразия
                vector<Vertex> sorted_targets = targets;
                sort(sorted_targets.begin(), sorted_targets.end());
                
                for (size_t j = 0; j < sorted_targets.size(); ++j) {
                    ss << " " << sorted_targets[j];
                }
                
                // Добавляем запятую и отступ, если это не последний буфер
                if (i < sorted_buffers.size() - 1) {
                    ss << ",      ";
                }
            }
        } else {
            // Если у вершины нет исходящих ребер
            ss << "0:";
        }
        
        content.push_back(ss.str());
    }
    
    return content;
}


struct SPGraph {
    std::vector<std::string> V;
    std::vector<std::pair<std::string, std::string>> E;
    std::map<std::string, int> omega;
    std::string source;
    std::string target;
    
    SPGraph(const std::vector<std::string>& vertices, 
            const std::vector<std::pair<std::string, std::string>>& edges,
            const std::map<std::string, int>& weights,
            const std::string& src, const std::string& tgt);
    
    friend std::ostream& operator<<(std::ostream& os, const SPGraph& graph);
};

struct DecompNode {
    std::string type;
    std::shared_ptr<SPGraph> graph;
    std::vector<std::shared_ptr<DecompNode>> components;
    
    DecompNode(const std::string& t, std::shared_ptr<SPGraph> g);
};

// Вспомогательные функции
std::vector<std::string> set_union(const std::vector<std::string>& v1, const std::vector<std::string>& v2);
std::vector<std::pair<std::string, std::string>> set_union_edges(
    const std::vector<std::pair<std::string, std::string>>& e1, 
    const std::vector<std::pair<std::string, std::string>>& e2);

// Основные алгоритмы
std::shared_ptr<DecompNode> decompose_sp_graph(std::shared_ptr<SPGraph> graph);
std::pair<std::vector<std::string>, std::vector<std::pair<std::string, std::string>>> 
print_decomposition_tree(std::shared_ptr<DecompNode> decomp_tree, int level = 0);
std::shared_ptr<SPGraph> Linearize(std::shared_ptr<SPGraph> G, const std::vector<std::string>& pi);
std::vector<std::string> Tree_Schedule(std::shared_ptr<SPGraph> G, const std::vector<std::string>& S, const std::vector<std::string>& T);
std::vector<std::string> PC_Schedule(std::shared_ptr<SPGraph> G, const std::vector<std::string>& S, const std::vector<std::string>& T);
std::pair<std::vector<std::string>, std::pair<std::vector<std::string>, std::vector<std::string>>> 
SP_Schedule(std::shared_ptr<SPGraph> G);

// Функции парсинга
void parse_input(const std::vector<std::string>& content,
                std::map<int, std::vector<std::pair<int, std::set<int>>>>& dct,
                std::map<int, std::vector<std::pair<int, int>>>& reverse_dct);


SPGraph::SPGraph(const std::vector<std::string>& vertices, 
                 const std::vector<std::pair<std::string, std::string>>& edges,
                 const std::map<std::string, int>& weights,
                 const std::string& src, const std::string& tgt)
    : V(vertices), E(edges), omega(weights), source(src), target(tgt) {
    std::sort(V.begin(), V.end());
    std::sort(E.begin(), E.end());
}

std::ostream& operator<<(std::ostream& os, const SPGraph& graph) {
    os << "SPGraph(source='" << graph.source << "', target='" << graph.target << "')";
    return os;
}

DecompNode::DecompNode(const std::string& t, std::shared_ptr<SPGraph> g) 
    : type(t), graph(g) {}

std::vector<std::string> set_union(const std::vector<std::string>& v1, const std::vector<std::string>& v2) {
    std::set<std::string> s(v1.begin(), v1.end());
    s.insert(v2.begin(), v2.end());
    return std::vector<std::string>(s.begin(), s.end());
}

std::vector<std::pair<std::string, std::string>> set_union_edges(
    const std::vector<std::pair<std::string, std::string>>& e1, 
    const std::vector<std::pair<std::string, std::string>>& e2) {
    std::set<std::pair<std::string, std::string>> s(e1.begin(), e1.end());
    s.insert(e2.begin(), e2.end());
    return std::vector<std::pair<std::string, std::string>>(s.begin(), s.end());
}

std::shared_ptr<DecompNode> decompose_sp_graph(std::shared_ptr<SPGraph> graph) {
    using EdgeTuple = std::tuple<std::string, std::string, std::shared_ptr<DecompNode>>;
    std::vector<EdgeTuple> edges;
    
    // Инициализация списка рёбер с базовыми узлами
    for (const auto& e : graph->E) {
        auto edge_graph = std::make_shared<SPGraph>(
            std::vector<std::string>{e.first, e.second},
            std::vector<std::pair<std::string, std::string>>{e},
            graph->omega, e.first, e.second
        );
        auto node = std::make_shared<DecompNode>("base", edge_graph);
        edges.push_back(std::make_tuple(e.first, e.second, node));
    }
    
    std::set<std::string> vertices(graph->V.begin(), graph->V.end());
    
    bool changed = true;
    while (changed && edges.size() > 1) {
        changed = false;
        
        // Последовательная редукция
        for (auto it = vertices.begin(); it != vertices.end(); ) {
            std::string v = *it;
            if (v == graph->source || v == graph->target) {
                ++it;
                continue;
            }
            
            std::vector<EdgeTuple> in_edges, out_edges;
            for (const auto& e : edges) {
                if (std::get<1>(e) == v) in_edges.push_back(e);
                if (std::get<0>(e) == v) out_edges.push_back(e);
            }
            
            if (in_edges.size() == 1 && out_edges.size() == 1) {
                auto e_in = in_edges[0];
                auto e_out = out_edges[0];
                std::string u = std::get<0>(e_in);
                std::string w = std::get<1>(e_out);
                
                // Создаем последовательный узел
                std::vector<std::shared_ptr<DecompNode>> components = {
                    std::get<2>(e_in), std::get<2>(e_out)
                };
                
                auto series_graph = std::make_shared<SPGraph>(
                    std::vector<std::string>{u, v, w},
                    std::vector<std::pair<std::string, std::string>>{
                        std::make_pair(std::get<0>(e_in), std::get<1>(e_in)),
                        std::make_pair(std::get<0>(e_out), std::get<1>(e_out))
                    },
                    graph->omega, u, w
                );
                
                auto new_node = std::make_shared<DecompNode>("series", series_graph);
                new_node->components = components;
                
                // Удаляем старые рёбра и добавляем новое
                edges.erase(std::find(edges.begin(), edges.end(), e_in));
                edges.erase(std::find(edges.begin(), edges.end(), e_out));
                edges.push_back(std::make_tuple(u, w, new_node));
                it = vertices.erase(it);
                changed = true;
                break;
            } else {
                ++it;
            }
        }
        
        if (changed) continue;
            
        // Параллельная редукция
        std::map<std::pair<std::string, std::string>, std::vector<EdgeTuple>> edge_groups;
        for (const auto& e : edges) {
            auto key = std::make_pair(std::get<0>(e), std::get<1>(e));
            edge_groups[key].push_back(e);
        }
        
        for (const auto& [key, group] : edge_groups) {
            if (group.size() > 1) {
                auto [u, v] = key;
                
                // Создаем параллельный узел
                std::vector<std::shared_ptr<DecompNode>> components;
                for (const auto& e : group) {
                    components.push_back(std::get<2>(e));
                }
                
                auto parallel_graph = std::make_shared<SPGraph>(
                    std::vector<std::string>{u, v},
                    std::vector<std::pair<std::string, std::string>>{key},
                    graph->omega, u, v
                );
                
                auto new_node = std::make_shared<DecompNode>("parallel", parallel_graph);
                new_node->components = components;
                
                // Удаляем старые рёбра и добавляем новое
                for (const auto& e : group) {
                    edges.erase(std::find(edges.begin(), edges.end(), e));
                }
                edges.push_back(std::make_tuple(u, v, new_node));
                changed = true;
                break;
            }
        }
    }
    
    // После завершения редукций должно остаться одно ребро
    if (edges.size() == 1) {
        return std::get<2>(edges[0]);
    } else {
        // Если осталось несколько рёбер, создаём параллельную композицию
        std::vector<std::shared_ptr<DecompNode>> components;
        for (const auto& e : edges) {
            components.push_back(std::get<2>(e));
        }
        
        auto node = std::make_shared<DecompNode>("parallel", graph);
        node->components = components;
        return node;
    }
}

std::pair<std::vector<std::string>, std::vector<std::pair<std::string, std::string>>> 
print_decomposition_tree(std::shared_ptr<DecompNode> decomp_tree, int level) {
    std::string indent(level * 2, ' ');
    
    if (decomp_tree->type == "base") {
        auto g = decomp_tree->graph;
        return std::make_pair(g->V, g->E);
    } else if (decomp_tree->type == "series" || decomp_tree->type == "parallel") {
        auto g = decomp_tree->graph;
        std::vector<std::string> VV;
        std::vector<std::pair<std::string, std::string>> EE;
        
        for (const auto& comp : decomp_tree->components) {
            auto [VN, EN] = print_decomposition_tree(comp, level + 2);
            
            // Объединяем вершины
            std::set<std::string> v_set(VV.begin(), VV.end());
            v_set.insert(VN.begin(), VN.end());
            VV = std::vector<std::string>(v_set.begin(), v_set.end());
            
            // Объединяем рёбра
            std::set<std::pair<std::string, std::string>> e_set(EE.begin(), EE.end());
            e_set.insert(EN.begin(), EN.end());
            EE = std::vector<std::pair<std::string, std::string>>(e_set.begin(), e_set.end());
        }
        
        // Обновляем граф
        g->V = VV;
        g->E = EE;
        return std::make_pair(VV, EE);
    }
    
    return std::make_pair(std::vector<std::string>(), std::vector<std::pair<std::string, std::string>>());
}

std::shared_ptr<SPGraph> Linearize(std::shared_ptr<SPGraph> G, const std::vector<std::string>& pi) {
    G->E.clear();
    G->source = pi[0];
    G->target = pi.back();
    for (size_t i = 0; i < pi.size() - 1; i++) {
        G->E.push_back(std::make_pair(pi[i], pi[i + 1]));
    }
    return G;
}

std::vector<std::string> Tree_Schedule(std::shared_ptr<SPGraph> G, const std::vector<std::string>& S, const std::vector<std::string>& T) {
    auto V = G->V;
    auto E = G->E;
    auto dct = G->omega;
    auto source = G->source;
    auto target = G->target;
    
    // Построение дерева (определение родителей для каждой вершины)
    std::map<std::string, std::vector<std::string>> tree;
    for (const auto& v : V) {
        tree[v] = std::vector<std::string>();
    }
    for (const auto& [u, v] : E) {
        if (tree.find(v) != tree.end()) {
            tree[v].push_back(u);
        }
    }
    
    // Корень - target (сток)
    std::string root = target;
    
    // Вычисление весов только для вершин из V
    std::map<std::string, int> weights;
    for (const auto& v : V) {
        auto it = dct.find(v);
        weights[v] = (it != dct.end()) ? it->second : 0;
    }
    
    // Вычисление весов поддеревьев и τ(v)
    std::map<std::string, int> subtree_weights;
    
    std::function<int(std::string)> compute_subtree_weight;
    compute_subtree_weight = [&](std::string v) -> int {
        int w = weights[v];
        for (const auto& child : tree[v]) {
            w += compute_subtree_weight(child);
        }
        subtree_weights[v] = w;
        return w;
    };
    
    compute_subtree_weight(root);
    
    std::map<std::string, int> tau;
    for (const auto& v : V) {
        if (v == root) {
            tau[v] = subtree_weights[root] - weights[root];
        } else {
            tau[v] = subtree_weights[v];
        }
    }
    
    // Рекурсивная функция построения расписания
    std::function<std::vector<std::string>(std::string)> pebble_ordering;
    pebble_ordering = [&](std::string r) -> std::vector<std::string> {
        if (tree[r].empty()) {
            return {r};
        }
        
        std::vector<std::vector<std::string>> child_schedules;
        for (const auto& child : tree[r]) {
            child_schedules.push_back(pebble_ordering(child));
        }
        
        // Функция объединения расписаний детей
        auto combine = [&](const std::vector<std::vector<std::string>>& schedules, std::string root_node) -> std::vector<std::string> {
            using Segment = std::tuple<int, int, int, int>;
            using SegmentWithNodes = std::pair<int, std::vector<std::string>>;
            
            // Вычисление значений peb для расписания
            auto compute_pebble_values = [&](const std::vector<std::string>& schedule) -> std::vector<int> {
                std::vector<int> peb = {0};
                for (const auto& node : schedule) {
                    int prev_peb = peb.back();
                    int sum_tau_children = 0;
                    if (tree.find(node) != tree.end()) {
                        for (const auto& child : tree[node]) {
                            auto it = tau.find(child);
                            if (it != tau.end()) {
                                sum_tau_children += it->second;
                            }
                        }
                    }
                    auto it = tau.find(node);
                    int new_peb = prev_peb + ((it != tau.end()) ? it->second : 0) - sum_tau_children;
                    peb.push_back(new_peb);
                }
                return std::vector<int>(peb.begin() + 1, peb.end());
            };
            
            // Вычисление сегментов для расписания
            auto compute_segments = [&](const std::vector<std::string>& schedule) -> std::vector<Segment> {
                auto peb_values = compute_pebble_values(schedule);
                int n = schedule.size();
                std::vector<Segment> segments;
                int current_start = 0;
                
                while (current_start < n) {
                    // Нахождение hill
                    int max_val = *std::max_element(peb_values.begin() + current_start, peb_values.end());
                    int h_index = current_start;
                    for (int i = current_start; i < n; i++) {
                        if (peb_values[i] == max_val) {
                            h_index = i;
                        }
                    }
                    
                    // Нахождение valley
                    int min_val = *std::min_element(peb_values.begin() + h_index, peb_values.end());
                    int v_index = h_index;
                    for (int i = h_index; i < n; i++) {
                        if (peb_values[i] == min_val) {
                            v_index = i;
                        }
                    }
                    
                    segments.push_back(std::make_tuple(current_start, v_index, max_val, min_val));
                    current_start = v_index + 1;
                }
                
                return segments;
            };
            
            std::vector<SegmentWithNodes> all_segments;
            for (const auto& sched : schedules) {
                auto segments = compute_segments(sched);
                for (const auto& seg : segments) {
                    auto [start_idx, end_idx, hill, valley] = seg;
                    std::vector<std::string> segment_nodes(
                        sched.begin() + start_idx, 
                        sched.begin() + end_idx + 1
                    );
                    int segment_value = hill - valley;
                    all_segments.push_back(std::make_pair(segment_value, segment_nodes));
                }
            }
            
            // Сортировка сегментов по убыванию
            std::sort(all_segments.begin(), all_segments.end(), 
                     [](const SegmentWithNodes& a, const SegmentWithNodes& b) {
                         return a.first > b.first;
                     });
            
            // Объединение сегментов
            std::vector<std::string> new_schedule;
            for (const auto& [value, nodes] : all_segments) {
                new_schedule.insert(new_schedule.end(), nodes.begin(), nodes.end());
            }
            new_schedule.push_back(root_node);
            return new_schedule;
        };
        
        return combine(child_schedules, r);
    };
    
    return pebble_ordering(root);
}

std::vector<std::string> PC_Schedule(std::shared_ptr<SPGraph> G, const std::vector<std::string>& S, const std::vector<std::string>& T) {
    // Создание G_rev_S
    std::map<std::string, int> omega_rev;
    for (const auto& [key, value] : G->omega) {
        omega_rev[key] = -1 * value;
    }
    
    auto G_rev_S = std::make_shared<SPGraph>(S, std::vector<std::pair<std::string, std::string>>(), 
                                            omega_rev, "", G->source);
    for (const auto& edge : G->E) {
        if (std::find(S.begin(), S.end(), edge.second) != S.end()) {
            G_rev_S->E.push_back(std::make_pair(edge.second, edge.first));
        }
    }
    
    // Создание G_T
    auto G_T = std::make_shared<SPGraph>(T, std::vector<std::pair<std::string, std::string>>(), 
                                        G->omega, "", G->target);
    for (const auto& edge : G->E) {
        if (std::find(T.begin(), T.end(), edge.first) != T.end()) {
            G_T->E.push_back(edge);
        }
    }
    
    auto sigma_rev = Tree_Schedule(G_rev_S, S, T);
    auto tau = Tree_Schedule(G_T, S, T);
    
    std::reverse(sigma_rev.begin(), sigma_rev.end());
    sigma_rev.insert(sigma_rev.end(), tau.begin(), tau.end());
    return sigma_rev;
}

std::pair<std::vector<std::string>, std::pair<std::vector<std::string>, std::vector<std::string>>> 
SP_Schedule(std::shared_ptr<SPGraph> G) {
    if (G->E.size() == 1) {
        return std::make_pair(
            std::vector<std::string>{G->E[0].first, G->E[0].second},
            std::make_pair(
                std::vector<std::string>{G->E[0].first},
                std::vector<std::string>{G->E[0].second}
            )
        );
    }
    
    auto decomposition1 = decompose_sp_graph(G);
    print_decomposition_tree(decomposition1);
    
    auto G1 = decomposition1->components[0]->graph;
    auto G2 = decomposition1->components[1]->graph;
    
    for (size_t i = 2; i < decomposition1->components.size(); i++) {
        auto comp = decomposition1->components[i];
        // Объединяем вершины
        std::set<std::string> v_set(G2->V.begin(), G2->V.end());
        v_set.insert(comp->graph->V.begin(), comp->graph->V.end());
        G2->V = std::vector<std::string>(v_set.begin(), v_set.end());
        
        // Объединяем рёбра
        std::set<std::pair<std::string, std::string>> e_set(G2->E.begin(), G2->E.end());
        e_set.insert(comp->graph->E.begin(), comp->graph->E.end());
        G2->E = std::vector<std::pair<std::string, std::string>>(e_set.begin(), e_set.end());
    }
    
    auto res1 = SP_Schedule(G1);
    auto pi1 = res1.first;
    auto S1_T1 = res1.second;
    auto S1 = S1_T1.first;
    auto T1 = S1_T1.second;
    
    auto res2 = SP_Schedule(G2);
    auto pi2 = res2.first;
    auto S2_T2 = res2.second;
    auto S2 = S2_T2.first;
    auto T2 = S2_T2.second;
    
    if (decomposition1->type == "series") {
        int omega1 = 0;
        for (const auto& vrtx : S1) {
            auto it = G->omega.find(vrtx);
            if (it != G->omega.end()) {
                omega1 += it->second;
            }
        }
        
        int omega2 = 0;
        std::vector<std::string> union_vertices;
        std::set<std::string> union_set(G1->V.begin(), G1->V.end());
        union_set.insert(S2.begin(), S2.end());
        union_vertices.assign(union_set.begin(), union_set.end());
        
        for (const auto& vrtx : union_vertices) {
            auto it = G->omega.find(vrtx);
            if (it != G->omega.end()) {
                omega2 += it->second;
            }
        }
        
        if (omega1 < omega2) {
            std::vector<std::string> new_pi = pi1;
            new_pi.insert(new_pi.end(), pi2.begin() + 1, pi2.end());
            
            std::vector<std::string> new_T;
            std::set<std::string> t_set(G2->V.begin(), G2->V.end());
            t_set.insert(T1.begin(), T1.end());
            new_T.assign(t_set.begin(), t_set.end());
            
            return std::make_pair(new_pi, std::make_pair(S1, new_T));
        } else {
            std::vector<std::string> new_pi = pi1;
            new_pi.insert(new_pi.end(), pi2.begin() + 1, pi2.end());
            
            std::vector<std::string> new_S;
            std::set<std::string> s_set(G1->V.begin(), G1->V.end());
            s_set.insert(S2.begin(), S2.end());
            new_S.assign(s_set.begin(), s_set.end());
            
            return std::make_pair(new_pi, std::make_pair(new_S, T2));
        }
    } else {
        auto G1_tilda = Linearize(G1, pi1);
        auto G2_tilda = Linearize(G2, pi2);
        
        std::vector<std::string> S, T;
        std::set<std::string> s_set(S1.begin(), S1.end());
        s_set.insert(S2.begin(), S2.end());
        S.assign(s_set.begin(), s_set.end());
        
        std::set<std::string> t_set(T1.begin(), T1.end());
        t_set.insert(T2.begin(), T2.end());
        T.assign(t_set.begin(), t_set.end());
        
        auto G_union = std::make_shared<SPGraph>(
            set_union(G1_tilda->V, G2_tilda->V),
            set_union_edges(G1_tilda->E, G2_tilda->E),
            G->omega, G2_tilda->source, G2_tilda->target
        );
        
        auto pi = PC_Schedule(G_union, S, T);
        return std::make_pair(pi, std::make_pair(S, T));
    }
}

void parse_input(const std::vector<std::string>& content,
                std::map<int, std::vector<std::pair<int, std::set<int>>>>& dct,
                std::map<int, std::vector<std::pair<int, int>>>& reverse_dct) {
    
    for (const auto& line : content) {
        if (line.empty()) continue;
        
        std::istringstream iss(line);
        int cur_v;
        iss >> cur_v;
        
        if (reverse_dct.find(cur_v) == reverse_dct.end()) {
            reverse_dct[cur_v] = std::vector<std::pair<int, int>>();
        }
        
        std::string rest;
        std::getline(iss, rest);
        
        std::vector<std::string> parts;
        std::istringstream rest_iss(rest);
        std::string part;
        while (std::getline(rest_iss, part, ',')) {
            parts.push_back(part);
        }
        
        std::vector<std::pair<int, std::set<int>>> buffers;
        for (const auto& p : parts) {
            std::istringstream p_iss(p);
            std::vector<std::string> buf_v;
            std::string token;
            while (p_iss >> token) {
                buf_v.push_back(token);
            }
            
            if (buf_v.empty()) continue;
            
            int weight = std::stoi(buf_v[0].substr(0, buf_v[0].size() - 1));
            std::set<int> descendants;
            
            for (size_t i = 1; i < buf_v.size(); i++) {
                int vert = std::stoi(buf_v[i]);
                descendants.insert(vert);
                
                if (reverse_dct.find(vert) == reverse_dct.end()) {
                    reverse_dct[vert] = {std::make_pair(weight, cur_v)};
                } else {
                    reverse_dct[vert].push_back(std::make_pair(weight, cur_v));
                }
            }
            
            buffers.push_back(std::make_pair(weight, descendants));
        }
        
        dct[cur_v] = buffers;
    }
}

namespace scheduling_problem::algorithms
{
    SeriesParallel::SeriesParallel(const std::string &label) : BaseOptimization(label) {}

    std::unique_ptr<BaseOptimization> SeriesParallel::copy() const
    {
        return std::unique_ptr<SeriesParallel>(new SeriesParallel(label_));
    }

    Schedule SeriesParallel::schedule_(const Graph &graph)
    {
        std::cout << "SeriesParallel::schedule_" << std::endl;
        
        vector<string> content = graphToContentFormatted(graph);
        
        std::map<int, std::vector<std::pair<int, std::set<int>>>> dct;
        std::map<int, std::vector<std::pair<int, int>>> reverse_dct;
        
        parse_input(content, dct, reverse_dct);
        
        // Создание vertexes_with_ancestors
        std::map<int, std::set<int>> vertexes_with_ancestors;
        for (const auto& [key, value] : reverse_dct) {
            vertexes_with_ancestors[key] = std::set<int>();
            for (const auto& edge : value) {
                vertexes_with_ancestors[key].insert(edge.second);
            }
        }
        
        // Создание new_dct
        std::map<int, std::vector<std::pair<int, int>>> new_dct;
        for (const auto& [key, value] : dct) {
            std::vector<std::pair<int, int>> new_value;
            for (const auto& buf : value) {
                for (int vrtx : buf.second) {
                    new_value.push_back(std::make_pair(buf.first, vrtx));
                }
            }
            new_dct[key] = new_value;
        }
        
        // Создание cumulative структур
        std::map<std::string, int> cumulative_dct;
        std::vector<std::string> cumulative_V;
        std::vector<std::pair<std::string, std::string>> cumulative_E;
        
        for (const auto& [vrtx, value] : new_dct) {
            int omega = 0;
            for (const auto& edge : value) {
                omega += edge.first;
            }
            cumulative_dct[std::to_string(vrtx) + "_start"] = omega;
            
            omega = 0;
            if (reverse_dct.find(vrtx) != reverse_dct.end()) {
                for (const auto& edge : reverse_dct.at(vrtx)) {
                    omega -= edge.first;
                }
            }
            cumulative_dct[std::to_string(vrtx) + "_stop"] = omega;
            
            cumulative_V.push_back(std::to_string(vrtx) + "_start");
            cumulative_V.push_back(std::to_string(vrtx) + "_stop");
            cumulative_E.push_back(std::make_pair(std::to_string(vrtx) + "_start", std::to_string(vrtx) + "_stop"));
            
            for (const auto& edge : value) {
                cumulative_E.push_back(std::make_pair(
                    std::to_string(vrtx) + "_stop", 
                    std::to_string(edge.second) + "_start"
                ));
            }
        }
        
        // Топологическая сортировка
        std::vector<int> topo_sort;
        auto vertexes_with_ancestors_copy = vertexes_with_ancestors;
        
        while (topo_sort.size() < content.size()) {
            for (auto it = vertexes_with_ancestors_copy.begin(); it != vertexes_with_ancestors_copy.end(); ) {
                if (it->second.empty()) {
                    topo_sort.push_back(it->first);
                    int removed = it->first;
                    it = vertexes_with_ancestors_copy.erase(it);
                    
                    for (auto& [key, value] : vertexes_with_ancestors_copy) {
                        value.erase(removed);
                    }
                    break;
                } else {
                    ++it;
                }
            }
        }
        
        // Создание графа
        auto G = std::make_shared<SPGraph>(
            cumulative_V, cumulative_E, cumulative_dct,
            std::to_string(topo_sort[0]) + "_start",
            std::to_string(topo_sort.back()) + "_stop"
        );
        
        auto result = SP_Schedule(G);
        auto pi = result.first;
        
        // Вычисление результатов - ИСПРАВЛЕННАЯ ЧАСТЬ согласно Python коду
        int max_f = 0;
        int f = 0;
        std::vector<int> schedule;
        std::vector<std::vector<int>> schedule_pord;
        
        for (const auto& i : pi) {
            // Извлекаем номер вершины (часть до '_')
            size_t underscore_pos = i.find('_');
            int vertex_num = std::stoi(i.substr(0, underscore_pos));
            
            if (i.find("start") != std::string::npos) {
                // Для stop вершин (оканчиваются на "_stop")
                schedule.push_back(vertex_num);
                schedule_pord.push_back({vertex_num, cumulative_dct[i]});
            } else {
                // Для start вершин (оканчиваются на "_start")
                schedule_pord.back().push_back(-1 * cumulative_dct[i]);
            }
            f += cumulative_dct[i];
            max_f = std::max(max_f, f);
        }
        
        // Вывод результатов
        std::cout << "Schedule: ";
        for (int v : schedule) {
            std::cout << v << " ";
        }
        std::cout << "\nMax flow: " << max_f << std::endl;
        
        for (const auto& item : schedule_pord) {
            std::cout << item[0] << ' ' << item[1] << ' ' << item[2] << std::endl;
        }
        
        Schedule out(0, graph.name());
        for (const auto& item : schedule_pord) {
            out.push(item[0], item[1], item[2]);
        }
        out.cost(true);
        return out;
        
    }
}
