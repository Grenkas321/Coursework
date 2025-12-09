#include "Greedy.h"
#include <algorithm>
#include <boost/range/irange.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "general_types.h"

#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <climits>

namespace scheduling_problem::algorithms {


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

// Глобальная структура данных для хранения информации о вершинах
// dct[vertex] = vector<pair<int, set<int>>> где:
// - первый элемент пары - вес буфера
// - второй элемент - множество потомков
map<int, vector<pair<int, set<int>>>> dct;

// Вспомогательная функция для разделения строки
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// Функция для удаления пробелов в начале и конце строки
string trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    if (start == string::npos || end == string::npos) {
        return "";
    }
    return str.substr(start, end - start + 1);
}

// Функция целевой функции
long long goal_function(const std::vector<int>& part_sched, int kk = -1) {
    long long f_hp = 0, f_hp_kk = 0;
    // kk = -1;
    for (size_t k = 0; k < part_sched.size(); k++) {
        long long f_hp_k = 0;
        
        // Первая сумма
        for (size_t i = 0; i <= k; i++) {
            int vertex = part_sched[i];
            for (const auto& buf : dct[vertex]) {
                f_hp_k += buf.first;
            }
        }
        
        // Вторая сумма (вычитание)
        for (size_t i = 0; i < k; i++) {
            int vertex = part_sched[i];
            const auto& cur_list = dct[vertex];
            for (const auto& buf : cur_list) {
                bool all_in_schedule = true;
                for (int desc : buf.second) {
                    bool found = false;
                    for (size_t idx = 0; idx < k; idx++) {
                        if (part_sched[idx] == desc) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        all_in_schedule = false;
                        break;
                    }
                }
                if (all_in_schedule) {
                    f_hp_k -= buf.first;
                }
            }
        }
        if (k >= kk and kk != -1) {
            f_hp_kk = std::max(f_hp_kk, f_hp_k);
        }
        f_hp = std::max(f_hp, f_hp_k);
    }
    if (kk != -1) {
        return f_hp_kk;
    }
    return f_hp;
}

bool allElementsInVector(const set<int>& elements, const vector<int>& vec, int k) {
    for (int elem : elements) {
        bool found = false;
        for (int i = 0; i < k; i++) {
            if (vec[i] == elem) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

Greedy::Greedy(const std::string& label)
    : BaseOptimization(label) {}

std::unique_ptr<BaseOptimization> Greedy::copy() const {
    return std::unique_ptr<BaseOptimization>(new Greedy(*this));
}

// ВАЖНО: без дефолтного ctor ScheduleStatus. Используем локальный scope,
// чтобы рабочий буфер был уничтожен ДО возврата (срез пика памяти).
Schedule Greedy::schedule_(const Graph &graph) {
    
    // std::cout << "Greedy::schedule_" << std::endl;
    /*
    auto vertex_range = boost::vertices(graph);
    for (auto it = vertex_range.first; it != vertex_range.second; ++it) {
        Vertex v = *it;
        std::cout << "Vertex: " << v << std::endl;
    }
    auto edge_range = boost::edges(graph);
    for (auto it = edge_range.first; it != edge_range.second; ++it) {
        auto e = *it;
        Vertex source = boost::source(e, graph);
        Vertex target = boost::target(e, graph);
        
        std::cout << "Edge: " << source << " -> " << target << std::endl;
    }
    for (auto e : boost::make_iterator_range(boost::edges(graph))) {
        Vertex source = boost::source(e, graph);
        Vertex target = boost::target(e, graph);
        
        int buffer_id = boost::get(edge_buffer_id_t(), graph, e);
        weight_t weight = boost::get(boost::edge_weight, graph, e);
        EdgeKind kind = boost::get(edge_kind_t(), graph, e);
        size_t edge_index = boost::get(boost::edge_index_t(), graph, e);
        
        std::cout << "Edge " << source << " -> " << target 
                  << ": buffer_id=" << buffer_id 
                  << ", weight=" << weight 
                  << ", kind=" << (kind == EdgeKind::Real ? "Real" : "Imaginary")
                  << ", index=" << edge_index << std::endl;
    }
    */
    
    
    // Имитация содержимого файла (аналог content из Python)
    /*
    vector<string> content = {
        "0            10: 1      3,      15: 2",
        "1             6: 4",
        "2             7: 4", 
        "3             9: 4",
        "4             0:"
    };
    */
    
    vector<string> content = graphToContentFormatted(graph);
    /*
    for (const string& line : content) {
        cout << line << endl;
    }
    */
    
    // Тип как требуется - vector пар
    std::vector<std::pair<int, std::set<int>>> vertexes_with_ancestors;
    dct.clear();

    // Парсинг данных
    for (size_t i = 0; i < content.size(); i++) {
        std::stringstream ss(content[i]);
        int cur_v;
        ss >> cur_v;
        
        // Проверяем, есть ли уже такая вершина в vertexes_with_ancestors
        bool vertex_exists = false;
        for (auto& pair : vertexes_with_ancestors) {
            if (pair.first == cur_v) {
                vertex_exists = true;
                break;
            }
        }
        if (!vertex_exists) {
            vertexes_with_ancestors.push_back({cur_v, std::set<int>()});
        }
        
        // Остальная часть строки
        std::string rest;
        std::getline(ss, rest);
        rest = rest.substr(rest.find_first_not_of(" "));
        
        std::vector<std::string> lst;
        size_t pos = 0;
        while ((pos = rest.find(',')) != std::string::npos) {
            lst.push_back(rest.substr(0, pos));
            rest = rest.substr(pos + 1);
        }
        lst.push_back(rest);
        
        std::vector<std::pair<int, std::set<int>>> buffers;
        
        for (const auto& buf_str : lst) {
            std::stringstream buf_ss(buf_str);
            std::string token;
            std::vector<std::string> buf_v;
            
            while (buf_ss >> token) {
                buf_v.push_back(token);
            }
            
            if (buf_v.empty()) continue;
            
            // Извлекаем число (убираем двоеточие)
            std::string num_str = buf_v[0];
            if (num_str.back() == ':') {
                num_str.pop_back();
            }
            int num = std::stoi(num_str);
            
            std::set<int> descendants;
            for (size_t j = 1; j < buf_v.size(); j++) {
                int vert = std::stoi(buf_v[j]);
                descendants.insert(vert);
                
                // Обновляем vertexes_with_ancestors
                bool desc_exists = false;
                for (auto& pair : vertexes_with_ancestors) {
                    if (pair.first == vert) {
                        pair.second.insert(cur_v);
                        desc_exists = true;
                        break;
                    }
                }
                if (!desc_exists) {
                    vertexes_with_ancestors.push_back({vert, {cur_v}});
                }
            }
            
            buffers.push_back({num, descendants});
        }
        
        dct[cur_v] = buffers;
    }
    /*
    // Вывод словаря для проверки
    std::cout << "vertexes_with_ancestors: " << std::endl;
    for (auto& [key, value] : vertexes_with_ancestors) {
        std::cout << key << ": {";
        bool first = true;
        for (int val : value) {
            if (!first) std::cout << ", ";
            std::cout << val;
            first = false;
        }
        std::cout << "}" << std::endl;
    }
    */
    // Копируем для использования позже
    auto dct2 = vertexes_with_ancestors;

    // Топологическая сортировка
    std::vector<int> topo_sort;
    auto temp_ancestors = vertexes_with_ancestors;
    
    while (topo_sort.size() < content.size()) {
        bool found = false;
        for (auto it = temp_ancestors.begin(); it != temp_ancestors.end(); ) {
            if (it->second.empty()) {
                int vertex = it->first;
                topo_sort.push_back(vertex);
                
                // Удаляем вершину из временного контейнера
                it = temp_ancestors.erase(it);
                found = true;
                
                // Удаляем эту вершину из множеств предков всех остальных вершин
                for (auto& pair : temp_ancestors) {
                    pair.second.erase(vertex);
                }
                break;
            } else {
                ++it;
            }
        }
        
        if (!found) {
            std::cout << "Цикл обнаружен!" << std::endl;
            break;
        }
    }
    /*
    for (int cur_v : topo_sort) {
        std::cout << cur_v << ' ';
    }
    std::cout << std::endl;
    */
    // Построение расписания
    std::vector<int> schedule;
    long long min_f = 0;

    for (int cur_v : topo_sort) {
        // Находим l_k
        int l_k = 0;
        for (int i = schedule.size() - 1; i >= 0; i--) {
            bool is_ancestor = false;
            for (const auto& pair : dct2) {
                if (pair.first == cur_v) {
                    if (pair.second.find(schedule[i]) != pair.second.end()) {
                        is_ancestor = true;
                        break;
                    }
                }
            }
            if (is_ancestor) {
                l_k = i + 1;
                break;
            }
        }

        std::vector<int> best_schedule;
        min_f = 0;

        for (size_t d = l_k; d <= schedule.size(); d++) {
            std::vector<int> test_schedule = schedule;
            test_schedule.insert(test_schedule.begin() + d, cur_v);
            long long f_value;
            if (d < schedule.size()) {
                f_value = goal_function(test_schedule, l_k);
            }
            else {
                f_value = goal_function(test_schedule);
            }
            
            if (best_schedule.empty()) {
                best_schedule = test_schedule;
                min_f = f_value;
            } else if (f_value < min_f) {
                best_schedule = test_schedule;
                min_f = f_value;
            }
        }
        
        schedule = best_schedule;
    }
    /*
    // Вывод результата
    std::cout << "Schedule: ";
    for (int v : schedule) {
        std::cout << v << " ";
    }
    std::cout << "with cost: " << min_f << std::endl;
    */
    // Дополнительные вычисления
    std::vector<long long> incr_mas = {0};
    std::vector<long long> decr_mas;
    std::vector<int> part_sched = schedule;
    
    for (size_t k = 0; k < part_sched.size(); k++) {
        long long f_hp_k = 0;
        
        for (size_t i = 0; i <= k; i++) {
            int vertex = part_sched[i];
            for (const auto& buf : dct[vertex]) {
                f_hp_k += buf.first;
            }
        }
        incr_mas.push_back(f_hp_k);
        
        long long decr = 0;
        for (size_t i = 0; i < k; i++) {
            int vertex = part_sched[i];
            const auto& cur_list = dct[vertex];
            for (const auto& buf : cur_list) {
                bool all_in_schedule = true;
                for (int desc : buf.second) {
                    bool found = false;
                    for (size_t idx = 0; idx < k; idx++) {
                        if (part_sched[idx] == desc) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        all_in_schedule = false;
                        break;
                    }
                }
                if (all_in_schedule) {
                    decr += buf.first;
                }
            }
        }
        decr_mas.push_back(decr);
    }
    
    decr_mas.push_back(incr_mas.back());
    /*
    for (size_t i = 0; i < schedule.size(); i++) {
        std::cout << schedule[i] << " " 
                  << incr_mas[i + 1] - incr_mas[i] << " " 
                  << decr_mas[i + 1] - decr_mas[i] << std::endl;
    }
    */
    Schedule out(0, graph.name());
    for (int i = 0; i < schedule.size(); i++) {
        out.push(schedule[i], incr_mas[i + 1] - incr_mas[i], decr_mas[i + 1] - decr_mas[i]);
    }
    out.cost(true);
    return out;
}

size_t Greedy::choice(const Graph &graph,
                      const ScheduleStatus &schedule,
                      size_t curr_vid)
{
    auto heu = heuInfo(graph, schedule, curr_vid);
    return std::max_element(heu.begin(), heu.end(),
                            [](const auto& a, const auto& b){
                                return a.second < b.second;
                            })->first;
}

// Heuristic: 1 / max_target после «вставки»
std::unordered_map<size_t,double>
Greedy::heuInfo(const Graph& graph, const ScheduleStatus& status, size_t curr_vid)
{
    const size_t lower = status.lower(curr_vid, graph);
    const size_t pos_count = status.size() + 1 - lower;

    std::unordered_map<size_t,double> out;
    out.reserve(pos_count);

    if (status.size() == 0) { out[0] = 1.0; return out; }

    std::vector<weight_t> targets(pos_count);
    weight_t stable_cost = 0, stable_target = 0;

    for (size_t pos = 0; pos < lower; ++pos) {
        stable_target += status[pos].volume;
        if (stable_target > stable_cost) stable_cost = stable_target;
        stable_target -= status[pos].release;
    }

    weight_t target = 0;
    const weight_t w_curr = boost::get(vertex_weight_t(), graph, curr_vid);
    for (size_t pos = 0; pos < status.size(); ++pos) {
        target += status[pos].volume;
        if (pos >= lower) targets[pos - lower] = target;
        target -= status[pos].release;
    }
    targets[pos_count - 1] = target + w_curr;

    weight_t base_max = std::max(stable_cost,
        *std::max_element(targets.begin(), targets.end()));
    out[status.size()] = 1.0 / std::max<weight_t>(1, base_max);

    size_t last_child = 0, child_remain = 0;
    weight_t curr_release = boost::out_degree(curr_vid, graph) ? 0 : w_curr;
    std::vector<std::pair<size_t, weight_t>> released;
    for (auto parent : status.parents(curr_vid, graph)) {
        std::tie(child_remain, last_child) = status.releaseOn(parent, graph);
        if (child_remain == 1) {
            const auto pw = boost::get(vertex_weight_t(), graph, parent);
            released.emplace_back(last_child, pw);
            curr_release += pw;
        }
    }
    std::sort(released.begin(), released.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });

    weight_t right_max = 0, left_max = 0;
    for (size_t curr_pos = status.size(); curr_pos-- > lower; ) {
        for (const auto& rel : released) {
            if (rel.first == curr_pos) curr_release -= rel.second;
            else if (rel.first > curr_pos) break;
        }
        const size_t idx = curr_pos - lower;
        const size_t next = idx + 1;

        targets[idx]  = targets[idx]  - status[curr_pos].volume + w_curr;
        targets[next] = targets[idx]  - curr_release + status[curr_pos].volume;

        if (targets[next] > right_max) right_max = targets[next];
        left_max = *std::max_element(targets.begin(), targets.begin() + next);

        const weight_t cost_max = std::max(left_max, right_max);
        out[curr_pos] = 1.0 / std::max<weight_t>(1, cost_max);
    }

    return out;
}

} // namespace scheduling_problem::algorithms

// For testing cherry-pick 
