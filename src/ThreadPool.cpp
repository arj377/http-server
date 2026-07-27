#include "ThreadPool.h"
#include <utility>


ThreadPool::ThreadPool(size_t numThreads) : done(false) { // Initialize done to false before constructor starts
    for (size_t i = 0; i < numThreads; i++) {
        workers.emplace_back([this] {
            while (true) {
                std::unique_lock<std::mutex> lock(queueMutex);
                cond.wait(lock, [this]{return done || !tasks.empty();}); // Wait if destructor is called or there is a task
                //if destructor is called and tasks are done, exit
                if (done && tasks.empty()) {
                    return;
                }
                auto task = std::move(tasks.front()); // Cheaper to move since we're popping anyway
                tasks.pop();
                lock.unlock();
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    std::unique_lock<std::mutex> lock(queueMutex); // Protect shared data
    done = true; 
    lock.unlock();
    cond.notify_all(); // Wake all threads
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join(); // Wait for all workers to finish
        }
    }
}