#include "ActiveObject.hpp"

template<typename T>
ActiveObject<T>::ActiveObject(std::function<void(std::shared_ptr<Message<T>>)> func)
    : running(true), processFunc(func) {
    // Start the worker thread, which will continuously process tasks from the queue
    worker = std::thread([this]() {
        while (true) {
            std::shared_ptr<Message<T>> msg;
            {
                // Lock the mutex to protect the task queue
                std::unique_lock<std::mutex> lock(this->mtx);
                
                // Wait for either new tasks or a stop signal
                this->cv.wait(lock, [this]() { return !this->tasks.empty() || !running; });
                
                // If running is false and the task queue is empty, exit the loop
                if (!running && this->tasks.empty()) {
                    return;
                }
                
                // Pop the next task from the queue
                msg = this->tasks.front();
                this->tasks.pop();
            }
            
            // Process the task using the provided processing function
            processFunc(msg);
        }
    });
}

template<typename T>
ActiveObject<T>::~ActiveObject() {
    // Stop the worker thread when the object is destroyed
    stop();
}

template<typename T>
void ActiveObject<T>::send(std::shared_ptr<Message<T>> msg) {
    {
        // Lock the mutex to safely add a task to the queue
        std::lock_guard<std::mutex> lock(mtx);
        
        // Add the new task to the task queue
        tasks.push(msg);
        
        // Notify one waiting thread (if any) that a new task is available
        cv.notify_one();
    }
}

template<typename T>
void ActiveObject<T>::stop() {
    {
        // Lock the mutex and set running to false, signaling the worker to stop
        std::lock_guard<std::mutex> lock(mtx);
        running = false;
        
        // Notify all threads to wake up, including the worker waiting for tasks
        cv.notify_all();
    }
    
    // If the worker thread is joinable, wait for it to finish execution
    if (worker.joinable()) {
        worker.join();
    }
}

// Explicit instantiation of the template for type double
template class ActiveObject<double>;