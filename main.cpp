#include "api.hpp"
#include "system.hpp"
#include "threadpool.hpp"
#include "command.hpp"
#include "version.hpp"
#include "options.hpp"

#include <string>
#include <list>
#include <vector>
#include <cstdio>

using namespace std;
using namespace worker;

typedef vector<string> arg_vec_t;
typedef arg_vec_t::const_iterator arg_vec_citer_t;
typedef list<string> arg_list_t;
typedef arg_list_t::const_iterator arg_list_citer_t;

int main(int argc, char **argv) {
    Options &options = parseOptions(argc, argv);
    
    if (options.version) {
        fprintf(stderr, "%s %u.%u.%u %s\n",
            WORKER_PROGRAM_NAME,
            WORKER_MAJOR_VERSION, WORKER_MINOR_VERSION, WORKER_REVISION,
            WORKER_POSTSCRIPT);
        return 2;
    }
    
    quiet = options.quiet;
    verbose = options.verbose;
    
    if (options.arguments.empty()) {
        Options::usage();
        return -2;
    }
    
    Debug("%s %u.%u.%u %s",
            WORKER_PROGRAM_NAME,
            WORKER_MAJOR_VERSION, WORKER_MINOR_VERSION, WORKER_REVISION,
            WORKER_POSTSCRIPT);
    Debug("Licensed under a modified MIT license, available at <github.com/bgotink/worker/blob/master/LICENSE.md>");
    
    Debug("Found %u cores, using maximally %u threads.", System::getNbCores(), options.nthreads);
    
    Command command(options.command);
    
    const uint nbPlaceholders = command.getNbPlaceholders();
    Debug("Parsed command with %u placeholders", nbPlaceholders);

    if (nbPlaceholders == 1) {
        arg_list_t jobArguments;
        
        for (Options::argiter_t i = options.arguments.begin(), e = options.arguments.end(); i != e; i++) {
            vector<string> tmpArgs = parseGlob(*i);
            jobArguments.insert(jobArguments.end(), tmpArgs.begin(), tmpArgs.end());
        }
        
        uint nbJobs = jobArguments.size();
        ThreadPool threadPool(min(options.nthreads, nbJobs), !options.showOutput);
        
        for (arg_list_citer_t i = jobArguments.begin(), e = jobArguments.end(); i != e; i++) {
            arg_vec_t currArg;
            currArg.push_back(*i);
        
            threadPool.schedule(command.fillArguments(currArg));
        }
        
        threadPool.join();
    } else { // nbPlaceholders != 1
        if (nbPlaceholders != options.arguments.size()) {
            Fatal("Invalid number of arguments given, "
                "expected %d placeholder arguments but got %d", nbPlaceholders, options.arguments.size());
        }

        arg_vec_t *jobArguments = new arg_vec_t[nbPlaceholders];
        
        uint idx = 0;
        for (Options::argiter_t i = options.arguments.begin(), e = options.arguments.end(); i != e; i++, idx++) {
            jobArguments[idx] = parseGlob(*i);
        }
        
        if (nbPlaceholders > 0) {
            uint nbJobs = jobArguments[0].size();
            for (uint i = 1; i < nbPlaceholders; i++) {
                if (jobArguments[i].size() != nbJobs) {
                    Fatal("Placeholder %u matches %u items while placeholder 0 matches %u items!", i, jobArguments[i].size(), nbJobs);
                }
            }
        }
        
        uint nbJobs = jobArguments[0].size();
        Debug("Using %u jobs", nbJobs);
        
        ThreadPool threadPool(min(options.nthreads, nbJobs), !options.showOutput);
        
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
