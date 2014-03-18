#include "api.hpp"
#include <stdarg.h>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <glob.h>
#include <mutex>
#include <thread>

using namespace std;

namespace worker {

    namespace exception {

        exception::exception() : str(NULL) {}

        exception::exception(const exception &o) : str(o.str) {
            o.str = NULL;
        }

        exception::~exception() throw() {
            if (str != NULL) {
                delete str;
                str = NULL;
            }
        }

        const char *exception::what() const throw() {
            if (str != NULL)
                return str;

            throw runtime_error("exception::what() not redefined and str not set!");
        }

    }

    static size_t ITOA_MAX_SIZE = static_cast<size_t>(ceil(log(numeric_limits<sint>::max())) + 2);
    string itoa(sint i) {
        char str[ITOA_MAX_SIZE];
        string retval;

        sprintf(str, "%d", i);
        retval = str;
        return retval;
    }

    std::vector<std::string> parseGlob(const std::string &str) {
        Debug("Parsing glob \"%s\"", str.c_str());
        glob_t matches;

        std::vector<std::string> result;

        int ret = glob(str.c_str(), GLOB_BRACE|GLOB_NOSORT|GLOB_TILDE, NULL, &matches);
        if (ret != 0) {
            if (ret == GLOB_NOMATCH) {
                Debug("found no matches... returning original string as match");

                result.push_back(str);
                return result;
            }

            throw invalid_argument("invalid placeholder");
        }

        Debug("matched %u files", matches.gl_pathc);

        result.reserve(matches.gl_pathc);

        for (size_t i = 0; i < matches.gl_pathc; i++) {
            result.push_back(matches.gl_pathv[i]);
        }

        globfree(&matches);
        return result;
    }

    bool quiet = false;
    bool verbose = false;

    mutex _processMutex;

    void Process(const char *format, va_list args, const char *message, bool shouldAbort = false) {
        unique_lock<mutex> lock(_processMutex);

#if defined(WORKER_IS_WINDOWS)
        char errorBuf[2048];
        vsnprintf_s(errorBuf, sizeof(errorBuf), _TRUNCATE, format, args);
#else
        char *errorBuf;
        if (vasprintf(&errorBuf, format, args) == -1) {
            cerr << "vasprintf() unable to allocate memory!" << endl;
            abort();
        }
#endif

        cerr << "[" << this_thread::get_id() << "]   \t";
        cerr << "[" << message << "]" << "\t" << errorBuf << endl;
        fflush(stderr);

#if !defined(PBRT_IS_WINDOWS)
        free(errorBuf);
#endif

        if (shouldAbort) {
#if defined(WORKER_IS_WINDOWS)
        __debugbreak();
#else
        abort();
#endif
        }
    }

    void Fatal(const char *format, ...) {
        va_list args;
        va_start(args, format);
        Process(format, args, "FATAL  ", true);
        va_end(args);
    }

    void Error(const char *format, ...) {
        va_list args;
        va_start(args, format);
        Process(format, args, "ERROR  ");
        va_end(args);
    }
    void Warn(const char *format, ...) {
        if (quiet && !verbose)
            return;

        va_list args;
        va_start(args, format);
        Process(format, args, "WARN   ");
        va_end(args);
    }
    void Info(const char *format, ...) {
        if (quiet && !verbose)
            return;

        va_list args;
        va_start(args, format);
        Process(format, args, "INFO   ");
        va_end(args);
    }
    void Debug(const char *format, ...) {
        if (!verbose)
            return;

        va_list args;
        va_start(args, format);
        Process(format, args, "DEBUG  ");
        va_end(args);
    }

    mutex _outputMutex;

    void Output(const char *output) {
        unique_lock<mutex> lock(_outputMutex);
        unique_lock<mutex> lock2(_processMutex);

        if (verbose)
            cerr << "[" << this_thread::get_id() << "]   \t[PROCESS]";
        cout << output << endl;
        fflush(stdout);
    }

}
