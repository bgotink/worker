#include "options.hpp"

#include "system.hpp"
#include "api.hpp"

#include <boost/program_options.hpp>
#include <sstream>
#include <cstdio>

using namespace std;

namespace worker {

    namespace po = ::boost::program_options;

    static const char * const usage = R"EOS(Usage: %1$s [options] <command> <argument> [argument ...]

%2$s
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

Notes:
  The default number of threads differs depending on your system, it is the same
  as the number of cores as returned by running
    %3$s

Example usage:
  The current directory contains story1, story1.part2, story2 and story2.part2.
  Running the following command will result in the files *.part2 being appended
  to the other files and being removed:
    %1$s -o 'cat {} >> {} && rm {0}' '*.part2' 'story{1,2}'

  The current directory contains story1.part1, story1.part2, story2.part1 and
  story2.part2. To create files story1 and story2 containing the entire stories:
    %1$s -o 'cat {} {0/%%1/2} > {0/%%.part1/}' \*.part1

  The current directory contains some png's you want to open using `xdg-open`,
  however, xdg-open doesn't support multiple arguments:
    %1$s 'xdg-open {}' '*.png'
  If there's only one placeholder and it is located at the end of the command,
  you can just leave it out:
    %1$s xdg-open '*.png'
  If there's only one placeholder, all other arguments will be seen as
  replacements:
    %1$s xdg-open *.png
  Note that this is equivalent to using 'find', except for the multi-threading.
    find . -maxdepth 1 -name \\*.png -exec xdg-open '{}' \\;
)EOS";

    Options::Options() {
    }

    static po::options_description *usage_options(NULL);
    static const char *program_name;

    static void createOptions() {
        if (usage_options != NULL) return;

        po::options_description &desc = *(usage_options = new po::options_description("Options"));

        desc.add_options()
            ("help,h", "produce this help message")
            ("verbose,v", "run verbose, shows program output")
            ("quiet,q", "run quiet, no program output")
            ("output,o", "show program output")
            ("nooutput,s", "do not show program output")
            ("nthreads,n", po::value<uint>()->default_value(System::getNbCores()), "the maximum number of threads to use")
            ("version", "print version info and exit");
    }

    void Options::usage() {
        createOptions();

        stringstream options_info;
        options_info << *usage_options;

        fprintf(stderr, ::worker::usage,
            program_name,
            options_info.str().c_str(),
#if defined(WORKER_IS_LINUX)
            "awk '/^processor/ {++n} END {print n+1}' /proc/cpuinfo"
#elif defined(WORKER_IS_OSX)
            "sysctl hw.ncpu"
#else
            "some command"
#endif
        );
    }

    Options &parseOptions(int argc, char **argv) {
        program_name = argv[0];
        createOptions();

        po::options_description cmdline_options("Command");
        cmdline_options.add_options()
            ("command", po::value<Options::command_t>(), "the command")
            ("arg", po::value<Options::arglist_t>(), "argument");

        cmdline_options.add(*usage_options);

        po::positional_options_description pod;
        pod.add("command", 1).add("arg", -1);

        po::variables_map vm;
        po::store(
            po::command_line_parser(argc, argv)
                .options(cmdline_options)
                .positional(pod)
                .run(),
            vm);

        if (vm.count("help")) {
            Options::usage();
            exit(1);
        }

        Options &options = *new Options;

        if (vm.count("version")) {
            options.version = true;
            return options;
        }
        options.version = false;

        // first the flags where verbosity AND output are set
        options.verbose = vm.count("verbose");
        options.quiet = options.verbose ? false : vm.count("quiet");

        // now program output
        if (vm.count("output") || vm.count("nooutput")) {
            options.showOutput = vm.count("output") && !vm.count("nooutput");
        } else {
            options.showOutput = options.verbose;
        }

        options.nthreads = vm.count("nthreads") ? vm["nthreads"].as<uint>() : System::getNbCores();

        if (!vm.count("command")) {
            Options::usage();
            exit(-2);
        }
        options.command = vm["command"].as<Options::command_t>();
        if (!vm.count("arg")) {
            Options::usage();
            exit(-4);
        }
        options.arguments = vm["arg"].as<Options::arglist_t>();

        return options;
    }
}
