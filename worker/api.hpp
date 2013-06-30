#ifndef __WORKER_API_
#define __WORKER_API_

#define APPNAME "Worker"
#define VERSION "0.1"

#if defined(_WIN32) || defined(_WIN64)
#define WORKER_IS_WINDOWS
#elif defined(__linux__)
#define WORKER_IS_LINUX
#elif defined(__OpenBSD__)
#define WORKER_IS_OPENBSD
#endif

#include <stdint.h>
typedef uint32_t uint;
typedef int32_t sint;

#include <string>
#include <vector>
#include <exception>

namespace worker {

    namespace exception {

        class exception : public std::exception {
        protected:
            mutable char *str;

        public:
            exception();
            exception(const exception &o);
            virtual ~exception() throw();
            virtual const char *what() const throw();
        };

    }

    std::string itoa(sint i);

    std::vector<std::string> parseGlob(const std::string &str);

    extern bool quiet;
    extern bool verbose;

    void Fatal(const char *format, ...);
    void Error(const char *format, ...);
    void Warn(const char *format, ...);
    void Info(const char *format, ...);
    void Debug(const char *format, ...);

    #define Assert(assertion) \
        ((assertion) ? (void)0 : \
            Fatal("Assertion \"%s\" failed in %s, line %d", \
                   #assertion, __FILE__, __LINE__))
}

#endif // !defined(__WORKER_API)