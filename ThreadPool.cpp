#include "ThreadPool.hpp"

ThreadPool::ThreadPool(size_t numThreads) : stopFlag(false) {
    // Create the specified number of worker threads
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this]() {
            while (true) {
                std::function<void()> task;
                {
                    // Lock the task queue
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    // Wait for a task to be available or for the stop flag to be set
                    this->condition.wait(lock, [this]() { return this->stopFlag || !this->tasks.empty(); });
                    // If the pool is stopping and there are no remaining tasks, exit the loop
                    if (this->stopFlag && this->tasks.empty())
                        return;
                    // Retrieve the next task from the queue
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                // Execute the task
                task();
            }
        });
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        // Lock the queue before adding a new task
        std::lock_guard<std::mutex> lock(queueMutex);
        // Prevent enqueuing tasks if the pool is stopped
        if (stopFlag)
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        // Add the task to the queue
        tasks.push(std::move(task));
        // Notify one waiting thread that a new task is available
        condition.notify_one(); // Notify while holding the lock
    }
}



void ThreadPool::stop() {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        stopFlag = true;  // Safely signal all threads to stop
        condition.notify_all();  // Wake up all threads
    }
    // Safely join all threads
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}


ThreadPool::~ThreadPool() {
    // Ensure the thread pool is stopped and cleaned up
    stop();
}