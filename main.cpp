#include "api.hpp"
#include "system.hpp"
#include "threadpool.hpp"
#include "command.hpp"

#include "string.h"
#include <string>
#include <cstdio>
#include <cstdlib>

using namespace std;

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
    if (!strcmp(str, pattern))
        return true;
    
    size_t len = strlen(pattern);
    for (size_t i = 0; i < len; i++) {
        if (str[i] != pattern[i])
            return false;
    }
    
    *begin = str + len;
    if (**begin == '=')
        (*begin)++;
    
    index++;
    return true;
}

int main(int argc, char **argv) {
    command = argv[0];
    
    if (argc == 1) {
        PrintUsage();
        return -1;
    }
    
    uint nbThreads = worker::System::getNbCores();
    
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
        } else
            break;
    }
    
    worker::Command command(argv[argpos]);
    const uint nbPlaceholders = command.getNbPlaceholders();
    argpos++;
    
    worker::Debug("Parsed command with %u placeholders", nbPlaceholders);

    if (nbPlaceholders == 1) {
        vector<string> jobArguments;
        
        for (; argpos < argc; argpos++) {
            vector<string> tmpArgs = worker::parseGlob(argv[argpos]);
            jobArguments.insert(jobArguments.end(), tmpArgs.begin(), tmpArgs.end());
        }
        
        uint nbJobs = jobArguments.size();
        
        worker::ThreadPool threadPool(min(nbThreads, nbJobs));
        
        for (uint i = 0; i < nbJobs; i++) {
            vector<string> currArg;
            currArg.push_back(jobArguments[i]);
        
            threadPool.schedule(command.fillArguments(currArg));
        }
        
        threadPool.join();
    } else { // nbPlaceholders != 1
        if (nbPlaceholders != static_cast<uint>(argc - argpos)) {
            worker::Fatal("Invalid number of arguments given, expected %d placeholder arguments but got %d", nbPlaceholders, argc - argpos);
        }

        vector<string> *jobArguments = new vector<string>[nbPlaceholders];
        
        for (uint idx = 0; argpos < argc; argpos++, idx++) {
            jobArguments[idx] = worker::parseGlob(argv[argpos]);
        }
        
        if (nbPlaceholders > 0) {
            uint nbJobs = jobArguments[0].size();
            for (uint i = 1; i < nbPlaceholders; i++) {
                if (jobArguments[i].size() != nbJobs) {
                    worker::Fatal("Placeholder %u matches %u items while placeholder 0 matches %u items!", i, jobArguments[i].size(), nbJobs);
                }
            }
        }
        
        uint nbJobs = (nbPlaceholders == 0) ? 1 : jobArguments[0].size();
        worker::Debug("Using %u jobs", nbJobs);
        
        worker::ThreadPool threadPool(min(nbThreads, nbJobs));
        
        for (uint i = 0; i < nbJobs; i++) {
            vector<string> thisArgs;
            
            for (uint j = 0; j < nbPlaceholders; j++)
                thisArgs.push_back(jobArguments[j][i]);
            
            threadPool.schedule(command.fillArguments(thisArgs));
        }
        
        delete[] jobArguments;
        
        threadPool.join();
    } // if (nbPlaceholders == 1)
}
