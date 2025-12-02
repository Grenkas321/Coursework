#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <mutex>
#include <condition_variable>

namespace scheduling_problem::additionals
{
    /**
     * @brief Fixed-size thread pool for concurrent task execution.
     *
     * Enqueues callable tasks and executes them on a set of worker threads.
     * Each enqueued task returns a std::future for retrieving its result.
     */
    class ThreadPool final
    {
    private:
        /**
         * @brief Internal pool status.
         */
        enum class status
        {
            /** Threads are initialized and idle/working. */
            available,
            /** Workers are running and may be processing tasks. */
            processing,
            /** Pool is shutting down; workers exit their loops. */
            stoped
        };

        using Task = std::function<void()>;

        std::vector<std::thread> threads_;
        std::queue<Task> tasks_;
        status pool_status_;
        std::mutex mtx_;
        std::condition_variable cond_;

    public:
        /**
         * @brief Construct a pool and start worker threads.
         *
         * @param n_threads Number of worker threads to create. Defaults to
         *                  std::thread::hardware_concurrency().
         */
        ThreadPool(size_t n_threads = std::thread::hardware_concurrency())
            : threads_(n_threads), pool_status_(status::available)
        {
            run();
        }

        /**
         * @brief Deleted copy constructor.
         */
        ThreadPool(ThreadPool const &) = delete;

        /**
         * @brief Deleted copy assignment operator.
         */
        ThreadPool operator=(const ThreadPool &other) = delete;

        /**
         * @brief Enqueue a callable to be executed by the pool.
         *
         * The callable must be invocable with no arguments and return a value
         * (including void). Returns a std::future that becomes ready when the
         * task finishes, yielding the callable's return value or completion.
         *
         * @tparam TCallBack  Callable type with signature R().
         * @param  func       Callable to execute.
         * @return std::future<R> for non-void R, or std::future<void> for void tasks.
         */
        template <class TCallBack>
        decltype(auto) enqueue(TCallBack &&func)
        {
            using ReturnType = decltype(func());
            auto promise = std::shared_ptr<std::promise<ReturnType>>(new std::promise<ReturnType>());
            auto result = promise->get_future();

            auto t = [prom = std::move(promise), task = std::move(func)]()
            { execute(*prom, task); };
            {
                std::lock_guard<std::mutex> lg(mtx_);
                tasks_.push(std::move(t));
            }

            cond_.notify_one();

            return result;
        }

        /**
         * @brief Check if the task queue is empty.
         * @return true if no tasks are waiting; false otherwise.
         */
        bool queueIsEmpty()
        {
            return tasks_.empty();
        }

        /**
         * @brief Destructor. Signals shutdown and joins worker threads.
         */
        ~ThreadPool()
        {
            stop();
        }

    private:
        /**
         * @brief Helper to execute a non-void task and fulfill its promise.
         * @tparam TReturnType Result type.
         * @tparam TCallBack   Callable type.
         * @param promise      Promise to fulfill with the task result.
         * @param task         Callable to execute.
         */
        template <class TReturnType, class TCallBack>
        static void execute(std::promise<TReturnType> &promise, TCallBack &task)
        {
            promise.set_value(task());
        }

        /**
         * @brief Helper to execute a void task and fulfill its promise.
         * @tparam TCallBack Callable type returning void.
         * @param promise    Promise to fulfill upon completion.
         * @param task       Callable to execute.
         */
        template <class TCallBack>
        static void execute(std::promise<void> &promise, TCallBack &task)
        {
            task();
            promise.set_value();
        }

        /**
         * @brief Start worker threads and set status to processing.
         */
        void run()
        {
            auto task_loop = [this]()
            {
                while (true)
                {
                    std::unique_lock<std::mutex> lock(mtx_);
                    cond_.wait(lock, [&]()
                               { return pool_status_ == status::stoped || !tasks_.empty(); });

                    if (pool_status_ == status::stoped)
                        break;

                    if (!tasks_.empty())
                    {
                        auto task = std::move(tasks_.front());
                        tasks_.pop();

                        lock.unlock();

                        task();
                    }
                    else
                        lock.unlock();
                }
            };

            for (auto &thread : threads_)
                thread = std::move(std::thread(task_loop));

            {
                std::lock_guard<std::mutex> lg(mtx_);
                pool_status_ = status::processing;
            }
        }

        /**
         * @brief Gracefully stop the pool and join all threads.
         *
         * Waits until the queue is drained, flips status to stoped, notifies
         * all workers, then joins all threads.
         */
        void stop()
        {
            while (true)
            {
                std::lock_guard<std::mutex> lg(mtx_);
                if (pool_status_ == status::processing && tasks_.empty())
                {
                    pool_status_ = status::stoped;
                    break;
                }
            }

            cond_.notify_all();

            for (auto &thread : threads_)
                thread.join();
        }
    };
}
