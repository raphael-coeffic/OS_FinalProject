
#ifndef ACTIVE_OBJECT_HPP
#define ACTIVE_OBJECT_HPP

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <memory>
#include <atomic>
#include "Message.hpp"

template<typename T>
class ActiveObject {
private:
    std::thread worker; // The worker thread that processes tasks
    std::queue<std::shared_ptr<Message<T>>> tasks; // Queue of tasks to be processed
    std::mutex mtx; // Mutex to protect access to the task queue
    std::condition_variable cv; // Condition variable to notify worker of new tasks
    std::atomic<bool> running; // Atomic flag to control the running state of the worker
    std::function<void(std::shared_ptr<Message<T>>)> processFunc; // Function to process each task

public:
    // Constructor that takes a processing function
    ActiveObject(std::function<void(std::shared_ptr<Message<T>>)> func);

    // Destructor that ensures the worker is stopped
    ~ActiveObject();

    // Method to send a new task to the active object
    void send(std::shared_ptr<Message<T>> msg);

    // Method to stop the active object and its worker thread
    void stop();
};

#endif // ACTIVE_OBJECT_HPP