#include <cstdlib>
#include <errno.h>

#include "api.hpp"
#include "system.hpp"

#if !defined(WORKER_IS_WINDOWS)
#include <sys/sysctl.h>
#endif

#if defined(WORKER_IS_LINUX) || defined(WORKER_IS_OPENBSD)
#include <unistd.h>
#endif

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/scoped_array.hpp>

using namespace std;
namespace io = boost::iostreams;

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
            Error("sysctlnametomib() failed.  Guessing 2 CPU cores.");
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

    void realexec(const string &command) {
        boost::scoped_array<char> cmd_writable(new char[command.size() + 1]);
        copy(command.begin(), command.end(), cmd_writable.get());
        cmd_writable[command.size()] = '\0';

        char arg0[] = "sh";
        char arg1[] = "-c";

        char * args[4];
        args[0] = arg0;
        args[1] = arg1;
        args[2] = cmd_writable.get();
        args[3] = NULL;

        execv("/bin/sh", args);
    }

    int System::exec(const string &command, bool quiet) {
        Debug("Executing %s", command.c_str());

        int pipe_fd[2];
        pipe(pipe_fd);
        Debug("Pipe created, reading from %d and writing to %d", pipe_fd[0], pipe_fd[1]);

        pid_t exec_pid;

        if ((exec_pid = fork()) == 0) {
            // close reading end of the pipe
            close(pipe_fd[0]);

            // replace stdout and stderr
            dup2(pipe_fd[1], 1);
            dup2(pipe_fd[1], 2);

            // close pipe
            close(pipe_fd[1]);

            // close stdin
            close(0);

            realexec(command);

            exit(errno);
        }

        if (exec_pid < 0) {
            Fatal("Failed to fork, aborting...");
        }

        // close writing end
        close(pipe_fd[1]);

        Debug("Forked with pid %d, start listening to output", exec_pid);

        io::stream<io::file_descriptor_source> cmd_output(pipe_fd[0], io::never_close_handle);
        string line;

        while (cmd_output.good()) {
            getline(cmd_output, line);

            if (!quiet && (!cmd_output.eof() || line.size())) {
                Output(line.c_str());
            }
        }
        if (cmd_output.eof()) {
            Debug("Process output closed.");
        } else {
            Error("Error occured when trying to read process output");
        }

        close(pipe_fd[0]);

        Debug("Waiting for pid to die");

        int result;
        waitpid(exec_pid, &result, 0);

        if (result == 0) {
            Debug("Process exited with success status");
        } else {
            Warn("Process exited with non-zero exit status: %d", result);
        }

        return result;
    }

}
