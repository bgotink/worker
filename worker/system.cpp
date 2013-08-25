#include <cstdlib>

#include "api.hpp"
#include "system.hpp"

#if !defined(WORKER_IS_WINDOWS)
#include <sys/sysctl.h>
#endif

#if defined(WORKER_IS_LINUX)
#include <unistd.h>
#endif

using namespace std;

namespace worker {

    uint System::getNbCores() {
    #if defined(WORKER_IS_WINDOWS)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #elif defined(WORKER_IS_LINUX)
        return sysconf(_SC_NPROCESSORS_ONLN);
    #else
        // mac/bsds
    #if defined(WORKER_IS_OPENBSD)
        int mib[2] = { CTL_HW, HW_NCPU };
    #else
        int mib[2];
        mib[0] = CTL_HW;
        size_t length = 2;
        if (sysctlnametomib("hw.logicalcpu", mib, &length) == -1) {
            Error("sysctlnametomib() filed.  Guessing 2 CPU cores.");
            return 2;
        }
        Assert(length == 2);
    #endif
        int nCores = 0;
        size_t size = sizeof(nCores);

        /* get the number of CPUs from the system */
        if (sysctl(mib, 2, &nCores, &size, NULL, 0) == -1) {
            Error("sysctl() to find number of cores present failed");
            return 2;
        }
        return nCores;
    #endif
    }

    int System::exec(const string &command, bool quiet) {
        Debug("Executing %s", command.c_str());
        if (quiet)
            return system((command + " >/dev/null 2>&1 </dev/null").c_str());
        else
            return system(command.c_str());
    }

}
