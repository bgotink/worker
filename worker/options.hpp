#ifndef _WORKER_OPTIONS
#define _WORKER_OPTIONS

#include <string>
#include <vector>

#include "api.hpp"

namespace worker {

    struct Options {
        typedef std::string command_t;
        typedef std::string argument_t;
        
        typedef std::vector<argument_t> arglist_t;
        typedef arglist_t::iterator argiter_t;
        
        bool verbose;
        bool quiet;
        bool showOutput;
        
        bool version;
        
        uint nthreads;
        
        command_t command;
        arglist_t arguments;
        
        static void usage();
        
    private:
        Options();
        
        friend Options &parseOptions(int, char**);
    };
    
    Options &parseOptions(int argc, char **argv);

}

#endif // defined(_WORKER_OPTIONS)