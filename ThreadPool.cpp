#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t numThreads) : stopFlag(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->mutex);

                    // Wait until there is a task or the pool is stopped
                    while (!stopFlag && tasks.empty()) {
                        condition.wait(lock);
                    }

                    if (stopFlag && tasks.empty())
                        return;

                    // The leader takes the task
                    task = std::move(tasks.front());
                    tasks.pop();
                }

                // Execute the task
                task();

                // Pass the leadership
                {
                    std::unique_lock<std::mutex> lock(this->mutex);
                    if (!tasks.empty()) {
                        // Notify the next thread to become the leader
                        condition.notify_one();
                    }
                }
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (stopFlag)
            throw std::runtime_error("Enqueue on a stopped ThreadPool");

        tasks.push(std::move(task));

        // Notify a thread to become the leader
        condition.notify_one();
    }
}

void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopFlag = true;
        condition.notify_all();
    }

    for (std::thread &worker : workers) {
        if (worker.joinable())
            worker.join();
    }
}

ThreadPool::~ThreadPool() {
    stop();
}
