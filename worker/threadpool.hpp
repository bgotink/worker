#ifndef __WORKER_THREADPOOL_
#define __WORKER_THREADPOOL_

#include <string>
#include <queue>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "api.hpp"

namespace worker {
    
    struct ThreadPool;
    
    namespace impl {
        void execute(ThreadPool &pool, std::thread &thread);
    }

    struct ThreadPool {
        
        ThreadPool(uint size);
        ~ThreadPool();
        
        void schedule(std::string command);
        void join() const;
        void terminate();
        
        bool isJoining() const;
        bool isJoined() const;
        
    private:
        // no copying!
        ThreadPool(const ThreadPool& o);
        
        typedef std::thread                 thread_t;
        typedef std::mutex                  mutex_t;
        typedef std::unique_lock<mutex_t>   lock_t;
        typedef std::condition_variable     condition_var_t;
        
        typedef std::queue<std::string>     queue_t;
        
        thread_t * const threads;
        const uint size;
        
        uint nbThreadsAlive;
        mutable mutex_t nbTAMutex;
        
        mutable condition_var_t thread_nop;
        mutable condition_var_t joinCV;
        
        queue_t queue;
        mutable mutex_t queueMutex;
        
        mutable bool joining;
        bool terminating;
        mutable bool joined;
        mutable mutex_t joinMutex;
        
        void setThreadFinished();
        
        bool isTerminating() const;
        bool isQueueEmpty() const;
        
        std::string getNextCommand();
        
        friend void impl::execute(ThreadPool&,std::thread&);
    };

}

#endif // !defined(__WORKER_THREADPOOL_)