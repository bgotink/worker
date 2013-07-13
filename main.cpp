#include "api.hpp"
#include "system.hpp"
#include "threadpool.hpp"
#include "command.hpp"
#include "version.hpp"

#include "string.h"
#include <string>
#include <list>
#include <vector>
#include <cstdio>
#include <cstdlib>

using namespace std;

typedef vector<string> arg_vec_t;
typedef arg_vec_t::const_iterator arg_vec_citer_t;

typedef list<string> arg_list_t;
typedef arg_list_t::const_iterator arg_list_citer_t;

const char * usage = R"EOS(    Usage:
        %1$s [-v] [-q] [-n nthreads] [--] <command> <arguments...>
    
            -v              run verbose
            -q              run quiet
            -n nthreads     use nthreads threads
            --              end option parsing

    Placeholders:
        You can use {} or {i} with i a non-negative integer to refer
        to placeholders:
            - ls {}
            - cat {1} {0}
        BASH-style replacements are supported (note: NOT a regular expression):
            - cat {} > {0/e/o}      # replace the first 'e' with 'o'
            - cat {} > {0//e/o}     # replace all 'e' with 'o'
            - cat {} > {0/%%e/o}     # replace the final character
                                    # with 'o' if it is 'e'
    
    Example usage:
        The current directory contains story1, story1.part2, story2 and story2.part2.
        Running the following command will result in the files *.part2 being appended
        to the other files and being removed:
                %1$s 'cat {} >> {} && rm {0}' '*.part2' 'story{1,2}'

        The current directory contains story1.part1, story1.part2, story2.part1 and
        story2.part2. To create files story1 and story2 containing the entire stories:
                %1$s 'cat {} {0/%%1/2} > {0/%%.part1/}' \*.part1

        The current directory contains some png's you want to open using `xdg-open`,
        however, xdg-open doesn't support multiple arguments:
                %1$s 'xdg-open {}' '*.png'
        If there's only one placeholder and it is located at the end of the command,
        you can just leave it out:
                %1$s xdg-open '*.png'
        If there's only one placeholder, all other arguments will be seen as
        replacements:
                %1$s xdg-open *.png
)EOS";

char *command;
void PrintUsage() {
    fprintf(stderr, usage, command);
}

bool matchesNoArg(const char *str, const char *pattern) {
    return !strcmp(str, pattern);
}

bool matchesWithArg(const char *str, const char *pattern, const char **begin, int &index) {
    size_t len = strlen(pattern);
    if (strncmp(str, pattern, len))
        // str != pattern, excluding \0 at the end
        // --> return false;
        return false;
    
    if (*(str + len) == '\0') {
        // str ends -> *begin is the pointer to the value
        // (type --name value)
        index++;
    } else {
        // str doesn't end -> either type --name=value, --namevalue, -n=v or -nv
        *begin = str + len;
        if (**begin == '=')
            (*begin)++;
    }
    
    return true;
}

int main(int argc, char **argv) {
    command = argv[0];
    
    if (argc == 1) {
        PrintUsage();
        return -1;
    }
    
    uint nbCores = worker::System::getNbCores(), nbThreads = nbCores;
    
    arg_list_t arguments;
    
    int argpos = 1;
    for (; argpos < argc; argpos++) {
        const char *curr = argv[argpos];
        const char *next = (argpos == argc - 1) ? NULL : argv[argpos+1];
        
        if (matchesWithArg(curr, "-n", &next, argpos) || matchesWithArg(curr, "--nthreads", &next, argpos)) {
            if (next == NULL) {
                PrintUsage();
                worker::Fatal("No argument given for \"-n\"");
            }
            int _nbThreads = atoi(next);
            if (_nbThreads < 0) {
                worker::Fatal("Number of threads must be positive, not %d", _nbThreads);
            }
            nbThreads = static_cast<uint>(_nbThreads);
        } else
        if (matchesNoArg(curr, "-h") || matchesNoArg(curr, "--help")) {
            PrintUsage();
            return 1;
        } else
        if (matchesNoArg(curr, "-q") || matchesNoArg(curr, "--quiet")) {
            worker::quiet = true;
            worker::verbose = false;
        } else
        if (matchesNoArg(curr, "-v") || matchesNoArg(curr, "--verbose")) {
            worker::quiet = false;
            worker::verbose = true;
        } else
        if (matchesNoArg(curr, "--")) {
            argpos++;
            break;
        } else {
            arguments.push_back(curr);
        }
    }
    
    for (; argpos < argc; argpos++)
        arguments.push_back(argv[argpos]);
    
    if (arguments.empty()) {
        PrintUsage();
        return -2;
    }
    
    worker::Debug("%s %u.%u.%u %d",
            WORKER_PROGRAM_NAME,
            WORKER_MAJOR_VERSION, WORKER_MINOR_VERSION, WORKER_REVISION,
            WORKER_POSTSCRIPT);
    worker::Debug("Licensed under a modified MIT license, available at <github.com/bgotink/worker/blob/master/LICENSE.md>");
    
    worker::Debug("Found %u cores, using maximally %u threads.", nbCores, nbThreads);
    
    worker::Command command(arguments.front());
    arguments.pop_front();
    
    const uint nbPlaceholders = command.getNbPlaceholders();
    worker::Debug("Parsed command with %u placeholders", nbPlaceholders);

    if (nbPlaceholders == 1) {
        arg_list_t jobArguments;
        
        for (arg_list_citer_t i = arguments.begin(), e = arguments.end(); i != e; i++) {
            vector<string> tmpArgs = worker::parseGlob(*i);
            jobArguments.insert(jobArguments.end(), tmpArgs.begin(), tmpArgs.end());
        }
        
        uint nbJobs = jobArguments.size();
        worker::ThreadPool threadPool(min(nbThreads, nbJobs));
        
        for (arg_list_citer_t i = jobArguments.begin(), e = jobArguments.end(); i != e; i++) {
            arg_vec_t currArg;
            currArg.push_back(*i);
        
            threadPool.schedule(command.fillArguments(currArg));
        }
        
        threadPool.join();
    } else { // nbPlaceholders != 1
        if (nbPlaceholders != arguments.size()) {
            worker::Fatal("Invalid number of arguments given, expected %d placeholder arguments but got %d", nbPlaceholders, argc - argpos);
        }

        arg_vec_t *jobArguments = new arg_vec_t[nbPlaceholders];
        
        uint idx = 0;
        for (arg_list_citer_t i = arguments.begin(), e = arguments.end(); i != e; i++, idx++) {
            jobArguments[idx] = worker::parseGlob(*i);
        }
        
        if (nbPlaceholders > 0) {
            uint nbJobs = jobArguments[0].size();
            for (uint i = 1; i < nbPlaceholders; i++) {
                if (jobArguments[i].size() != nbJobs) {
                    worker::Fatal("Placeholder %u matches %u items while placeholder 0 matches %u items!", i, jobArguments[i].size(), nbJobs);
                }
            }
        }
        
        uint nbJobs = jobArguments[0].size();
        worker::Debug("Using %u jobs", nbJobs);
        
        worker::ThreadPool threadPool(min(nbThreads, nbJobs));
        
        for (uint i = 0; i < nbJobs; i++) {
            arg_vec_t thisArgs;
            
            for (uint j = 0; j < nbPlaceholders; j++)
                thisArgs.push_back(jobArguments[j][i]);
            
            threadPool.schedule(command.fillArguments(thisArgs));
        }
        
        delete[] jobArguments;
        
        threadPool.join();
    } // if (nbPlaceholders == 1)
}
