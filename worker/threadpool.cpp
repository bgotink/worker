#include "threadpool.hpp"
#include "system.hpp"
#include "api.hpp"

#include <sstream>
#include <functional>

using namespace std;

namespace worker {
    
    ThreadPool::ThreadPool(uint size) : threads(new thread[size]), size(size), nbThreadsAlive(size), joining(false), joined(false) {
        Debug("Creating threadpool with %u threads", size);
        for (uint i = 0; i < size; i++) {
            threads[i] = thread(impl::execute, ref(*this), ref(threads[i]));
        }
        Debug("ThreadPool initialised");
    }
    
    ThreadPool::~ThreadPool() {
        Debug("Destructing ThreadPool...");
        if (!isJoined())
            terminate();
        
        for (uint i = 0; i < size; i++)
            if (threads[i].joinable())
                threads[i].detach();
        
        delete[] threads;
        Debug("ThreadPool destructed");
    }
    
    // joining stuff
    
    void ThreadPool::terminate() {
        lock_t lock(joinMutex);
        Debug("ThreadPool::terminate() called");
        
        if (joined)
            return;

        if (!terminating) {
            terminating = true;
            thread_nop.notify_all();
        }
        
        bool threadsAlreadyEnded = false;
        {
            lock_t lockNbTA(nbTAMutex);
            if (nbThreadsAlive == 0)
                threadsAlreadyEnded = true;
        }
        
        if (!joining && !threadsAlreadyEnded) {
            Debug("Notifying threads that the ThreadPool is joining");
            joining = true;
            thread_nop.notify_all();
        }
        
        if (!threadsAlreadyEnded) {
            Debug("Waiting until join completes");
            joinCV.wait(lock, [this]{ return this->nbThreadsAlive == 0; });
            Debug("Waiting ended");
        }
        
        if (joined) {
            Debug("Already joined thread objects");
            return;
        }
        
        Debug("Joining thread objects");
        for (uint i = 0; i < size; i++)
            threads[i].join();
        Debug("Joining done");
        
        joining = false;
        joined = true;
    }
    
    void ThreadPool::join() const {
        lock_t lock(joinMutex);
        Debug("ThreadPool::join() called");
        
        if (joined)
            return;
        
        bool threadsAlreadyEnded = false;
        {
            lock_t lockNbTA(nbTAMutex);
            if (nbThreadsAlive == 0)
                threadsAlreadyEnded = true;
        }
        
        if (!joining && !threadsAlreadyEnded) {
            Debug("Notifying threads that the ThreadPool is joining");
            joining = true;
            thread_nop.notify_all();
        }
        
        if (!threadsAlreadyEnded) {
            Debug("Waiting until join completes");
            joinCV.wait(lock, [this]{ return this->nbThreadsAlive == 0; });
            Debug("Waiting ended");
        }
        
        if (joined) {
            Debug("Already joined thread objects");
            return;
        }
        
        Debug("Joining thread objects");
        for (uint i = 0; i < size; i++)
            threads[i].join();
        Debug("Joining done");
        
        joining = false;
        joined = true;
    }
    
    bool ThreadPool::isJoining() const {
        lock_t lock(joinMutex);
        return joining;
    }
    
    bool ThreadPool::isJoined() const {
        lock_t lock(joinMutex);
        return joined;
    }
    
    bool ThreadPool::isTerminating() const {
        lock_t lock(joinMutex);
        return terminating;
    }
    
    void ThreadPool::setThreadFinished() {
        lock_t lock(nbTAMutex);
        nbThreadsAlive --;
        Debug("Deactivating thread, active thread count = %u", nbThreadsAlive);
        
        if (nbThreadsAlive == 0)
            joinCV.notify_all();
    }
    
    // scheduling sutff
    
    void ThreadPool::schedule(string command) {
        lock_t lock(queueMutex);
        Debug("Scheduling \"%s\", %u commands in queue already", command.c_str(), queue.size());
        
        bool empty = queue.empty();
        queue.push(command);
        
        if (empty) {
            thread_nop.notify_all();
        }
    }
    
    string ThreadPool::getNextCommand() {
        lock_t lock(queueMutex);
        Debug("Requesting command, current queue size is %u", queue.size());
        
        if (queue.empty()) {
            if (!isJoining())
                thread_nop.wait(lock);
            return "";
        }
        
        Debug("queue size: %u", queue.size());
        string command(queue.front());
        queue.pop();
        
        return command;
    }
    
    bool ThreadPool::isQueueEmpty() const {
        lock_t lock(queueMutex);
        return queue.empty();
    }
    
    // run function
    
    namespace impl {
        void execute(ThreadPool &pool, thread &thread) {
            Debug("Thread started.");
        
            while(true) {
                if (pool.isJoining()) {
                    if (pool.isTerminating()) {
                        // terminating, ignore queue length
                        Debug("Terminating thread");
                        break;
                    }
                    
                    if (pool.isQueueEmpty()) {
                        // joining & queue is empty
                        Debug("Joining thread");
                        break;
                    } else {
                        Debug("Pool is joining but queue is not empty yet");
                    }

                    // joining but queue is not empty yet!
                }
            
                Debug("Trying to get next command");
                string command = pool.getNextCommand();
            
                if (command == "") {
                    Debug("Got empty command, continuing");
                    continue;
                }
            
                Debug("running command \"%s\"", command.c_str());
                int retval = System::exec(command + " > /dev/null 2>&1 < /dev/null");
                if (retval != 0)
                    Warn("command \"%s\" exited with code %d", command.c_str(), retval);
                else
                    Debug("Command \"%s\" executed successfully", command.c_str());
            }
        
            Debug("Thread shutting down.");
        
            pool.setThreadFinished();
            Debug("Thread ended");
        } // void execute(ThreadPool&)
    } // namespace impl
}