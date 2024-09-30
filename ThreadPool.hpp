#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <atomic>
#include <iostream>

class ThreadPool {
private:
    // Vector of worker threads
    std::vector<std::thread> workers;

    // Mutex for synchronizing access to the task queue
    std::mutex queueMutex;

    // Condition variable for worker thread notification
    std::condition_variable condition;

    // Queue of tasks (each task is a callable object)
    std::queue<std::function<void()>> tasks;

    // Flag to indicate whether the thread pool is stopping
    std::atomic<bool> stopFlag;

public:
    // Constructor that initializes the thread pool with a specified number of threads
    ThreadPool(size_t numThreads);

    // Destructor to clean up the thread pool
    ~ThreadPool();

    // Method to add a task to the queue for worker threads to execute
    void enqueue(std::function<void()> task);

    // Method to stop the thread pool and wait for all threads to finish
    void stop();
};

#endif // THREADPOOL_HPP