#pragma once

#include <set>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "DAGPool.h"
#include "general_types.h"   // важно: тут определён edge_buffer_id_t, vertex_weight_t и т.д.

namespace scheduling_problem::additionals
{
    /**
     * Reads given number of graphs from a specified directory.
     * Поддерживает два формата:
     *  1) Старый:  header "node size children" + строки "id size children..."
     *  2) Новый:   строки вида "id  w1: c1 c2,  w2: c3 ...", где каждая группа — отдельный буфер
     */
    class DAGReader : public DAGPool
    {
    private:
        std::string directory_;
        std::vector<std::filesystem::path> paths_;

    public:
        DAGReader(std::string directory,
                  unsigned n_samples,
                  unsigned batch_size);

        virtual Batch nextBatch();

        static Graph read(const std::string &filename);

    private:
        // true, если файл воспринимается как DAG (старый или новый формат)
        static bool isDAG(const std::string &filename);

        // Универсальный конструктор графа — определяет формат и зовёт нужный парсер
        static Graph make(const std::string &filename);

        // Парсер старого формата "node size children"
        static Graph makeOld(std::ifstream &file, const std::string &filename);

        // Парсер нового формата "id  w1: c... , w2: c..."
        static Graph makeNew(std::ifstream &file, const std::string &filename);

        // Приведение произвольных ID (в т.ч. отрицательных) к [0..N)
        static std::unordered_map<long long, size_t> cleanData(std::set<long long> &vertices);
    };
}
