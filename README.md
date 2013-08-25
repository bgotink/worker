## Requirements

* compiler
    * a GCC compiler supporting C++11 ```std::thread```, ```std::mutex```, ```std::condition_variable``` and multiline strings
    * Clang v3.3 or higher
* the [boost](http://boost.org) regex and program_options libraries
* make

To install the requirements on OS X using [homebrew](//github.com/mxcl/homebrew):

```
brew tap homebrew/versions
brew install make boost # clang is already available
```

## Building

run ```make```  
This has been tested on:

```
OS X 10.8 and 10.9, with GCC 4.9 and Clang, and libboost installed using brew
Ubuntu 12.04.2 LTS, with GCC 4.8.1 and Clang, libboost installed using APT
```

## Running

```
Usage: worker [options] <command> <argument> [argument ...]
        
Options:
  -h [ --help ]              produce this help message
  -v [ --verbose ]           run verbose, shows program output
  -q [ --quiet ]             run quiet, no program output
  -o [ --output ]            show program output
  -s [ --nooutput ]          do not show program output
  -n [ --nthreads ] arg (=8) the maximum number of threads to use
  --version                  print version info and exit

Placeholders:
  You can use {} or {i} with i a non-negative integer to refer
  to placeholders:
    - ls {}
    - cat {1} {0}
  BASH-style replacements are supported (note: NOT a regular expression):
    - cat {} > {0/e/o}      # replace the first 'e' with 'o'
    - cat {} > {0//e/o}     # replace all 'e' with 'o'
    - cat {} > {0/%e/o}     # replace the final character
                            # with 'o' if it is 'e'

Example usage:
  The current directory contains story1, story1.part2, story2 and story2.part2.
  Running the following command will result in the files *.part2 being appended
  to the other files and being removed:
    worker -o 'cat {} >> {} && rm {0}' '*.part2' 'story{1,2}'

  The current directory contains story1.part1, story1.part2, story2.part1 and
  story2.part2. To create files story1 and story2 containing the entire stories:
    worker -o 'cat {} {0/%1/2} > {0/%.part1/}' \*.part1

  The current directory contains some png's you want to open using `xdg-open`,
  however, xdg-open doesn't support multiple arguments:
    worker 'xdg-open {}' '*.png'
  If there's only one placeholder and it is located at the end of the command,
  you can just leave it out:
    worker xdg-open '*.png'
  If there's only one placeholder, all other arguments will be seen as
  replacements:
    worker xdg-open *.png

  Note that, due to the implementation of the exec call, the --output option must
  be given if the output is stored in a file, i.e. by using
    worker -o 'cat {} > combined_files' *
```

Run ```bin/worker``` to get an updated usage statement.  

## License

This program is released under a modified MIT license. For the license, see LICENSE.md.
