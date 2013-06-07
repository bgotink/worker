#ifndef __WORKER_SYSTEM_
#define __WORKER_SYSTEM_

#include <string>

namespace worker {

    struct System {
        
        static uint getNbCores();
        static  int exec(const std::string &command);
        
    private:
        System();
        ~System();
    };

}

#endif // !defined(__WORKER_SYSTEM_)