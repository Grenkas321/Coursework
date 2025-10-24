#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <mutex>

namespace scheduling_problem::additionals
{
    /**
     * Stores the pool of system threads that can be used for concurrent execution of algorithms
     */
    class ThreadPool final
    {
    private:
        /**
         * Status of each thread
         */
        enum class status
        {
            /**
             * Thread is avaliable
             */
            available,
            /**
             * Thread is being used
             */
            processing,
            /**
             * Thread is stopped
             */
            stoped
        };

        typedef std::function<void()> Task;

        std::vector<std::thread> threads_;
        std::queue<Task> tasks_;
        status pool_status_;
        std::mutex mtx_;
        std::condition_variable cond_;

    public:
        /**
         * Constructor
         */
        ThreadPool(size_t n_threads = std::thread::hardware_concurrency())
            : threads_(n_threads), pool_status_(status::available)
        {
            run();
        }

        /**
         * Copy constructor
         */
        ThreadPool(ThreadPool const &) = delete;
        /**
         * Operator =
         */
        ThreadPool operator=(const ThreadPool &other) = delete;

        /**
         * Add element to queue
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
         * Check if queue is empty
         */
        bool queueIsEmpty()
        {
            return tasks_.empty();
        }
        /**
         * Destructor
         */
        ~ThreadPool()
        {
            stop();
        }

    private:
        template <class TReturnType, class TCallBack>
        static void execute(std::promise<TReturnType> &promise, TCallBack &task)
        {
            promise.set_value(task());
        }

        template <class TCallBack>
        static void execute(std::promise<void> &promise, TCallBack &task)
        {
            task();
            promise.set_value();
        }

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