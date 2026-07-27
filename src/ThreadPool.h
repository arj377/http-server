#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>   // if you're storing std::function<void()>
#include <cstddef>      // for size_t

class ThreadPool {
public: 
    ThreadPool(size_t); //constructor
    ~ThreadPool(); //destructor
    
    template <class F>
    void enqueue(F&&); //adding tasks


private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable cond; 
    bool done;
};

template <class F>
void ThreadPool::enqueue(F&& task) {
    std::unique_lock<std::mutex> lock(queueMutex);
    tasks.emplace(std::forward<F>(task)); // Preserve if it wants to be copied or moved
    lock.unlock();
    cond.notify_one();
}
