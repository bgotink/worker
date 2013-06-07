#include "threadpool.hpp"
#include "system.hpp"
#include "api.hpp"

#include <sstream>
#include <functional>

using namespace std;

namespace worker {
    
    ThreadPool::ThreadPool(uint size) : threads(new thread[size]), size(size), joining(false), joined(false) {
        Debug("Creating threadpool with %u threads", size);
        for (uint i = 0; i < size; i++) {
            threads[i] = thread(impl::execute, ref(*this), ref(threads[i]));
        }
        Debug("ThreadPool initialised");
    }
    
    ThreadPool::~ThreadPool() {
        if (!isJoined())
            terminate();
        
        delete[] threads;
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
        
        join();
    }
    
    void ThreadPool::join() const {
        lock_t lock(joinMutex);
        Debug("ThreadPool::join() called");
        
        if (joined)
            return;
        
        {
            lock_t lockNbAT(nbATMutex);
            if (nbActiveThreads == 0)
                return;
        }
        
        if (!joining) {
            joining = true;
            thread_nop.notify_all();
        }
        
        joinCV.wait(lock);
        
        for (uint i = 0; i < size; i++) {
            if (threads[i].joinable()) {
                threads[i].detach();
            }
        }
        
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
    
    void ThreadPool::setThreadActive() {
        lock_t lock(nbATMutex);
        nbActiveThreads ++;
    }
    
    void ThreadPool::setThreadInactive() {
        lock_t lock(nbATMutex);
        nbActiveThreads --;
        
        if (nbActiveThreads == 0)
            joinCV.notify_all();
    }
    
    // scheduling sutff
    
    void ThreadPool::schedule(string command) {
        lock_t lock(queueMutex);
        Debug("Scheduling \"%s\", %u commands in queue already", command.c_str(), queue.size());
        
        bool empty = queue.empty();
        queue.push(command);
        
        if (empty) {
            thread_nop.notify_one();
        }
    }
    
    string ThreadPool::getNextCommand() {
        lock_t lock(queueMutex);
        Debug("Requesting command, current queue size is %u", queue.size());
        
        if (queue.empty()) {
            thread_nop.wait(lock);
            return "";
        }
        
        string command = queue.front();
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
            pool.setThreadActive();
        
            thread::id thread_id = this_thread::get_id();
            string _thread_id_str;
            {
                stringstream ss;
                ss << thread_id;
                _thread_id_str = ss.str();
            }
            const char * const thread_id_str = _thread_id_str.c_str();
            
            Debug("Thread %s started.", thread_id_str);
        
            while(true) {
                if (pool.isJoining()) {
                    if (pool.isTerminating()) {
                        // terminating, ignore queue length
                        Debug("Terminating thread %s", thread_id_str);
                        break;
                    }
                    
                    if (pool.isQueueEmpty()) {
                        // joining & queue is empty
                        Debug("Joining thread %s", thread_id_str);
                        break;
                    }

                    // joining but queue is not empty yet!
                }
            
                string command = pool.getNextCommand();
            
                if (command == "")
                    continue;
            
                Debug("Thread %s is running command \"%s\"", thread_id_str, command.c_str());
                int retval = System::exec(command);
                if (retval != 0)
                    Debug("Thread %s: command \"%s\" exited with code %d", thread_id_str, command.c_str(), retval);
            }
        
            Debug("Thread %s ended.", thread_id_str);
        
            pool.setThreadInactive();
            thread.detach();
        } // void execute(ThreadPool&)
    } // namespace impl
}