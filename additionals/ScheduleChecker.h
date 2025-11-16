#pragma once

#include <string>
#include "general_types.h"
#include "Schedule.h"

namespace scheduling_problem::additionals
{
    /**
     * Проверка корректности расписания в терминах «буферов на рёбрах».
     *
     * Модель:
     *  - Каждый буфер — это группа рёбер (parent -> child) с одинаковым (parent, buffer_id).
     *  - Вес буфера хранится в edge_weight, идентификатор буфера — в edge_buffer_id_t.
     *  - Память «поднимается» при выполнении продюсера (объём = сумма весов буферов вершины).
     *  - Буфер освобождается при выполнении последнего его потребителя.
     */
    class ScheduleChecker
    {
    public:
        /**
         * Полная проверка корректности «по буферам».
         * Возвращает true, если для каждой позиции расписания рассчитанный release
         * совпадает с записанным в Job::release, и инварианты соблюдены.
         *
         * @param graph   Входной граф
         * @param sched   Проверяемое расписание (Schedule или ScheduleStatus)
         * @param message Опционально — сюда будет записано пояснение при ошибке
         */
        static bool isCorrect(const Graph& graph,
                              const Schedule& sched,
                              std::string* message = nullptr);

        /**
         * Пересчёт пикового использования памяти по новой модели.
         * Удобно для сравнения с Schedule::cost(graph), если нужно.
         */
        static weight_t recomputePeak(const Graph& graph, const Schedule& sched);
    };
}
