## Requirements

* compiler
    * a GCC compiler supporting C++11 ```std::thread```, ```std::mutex```, ```std::condition_variable``` and multiline strings
    * Clang v3.3 or higher
* the [boost](http://boost.org) regex library
* make

To install the requirements on OS X using [homebrew](//github.com/mxcl/homebrew):

```
brew tap homebrew/versions
brew install make boost # clang is already available
```

## Building

run ```make```  
Please note that the Makefile has only been used on OS X 10.8 and 10.9, with GCC 4.9 and Clang, and libboost installed using brew.

## Running

```
Usage:
    bin/worker [-v] [-q] [-n nthreads] [--] <command> <arguments...>

        -v              run verbose
        -q              run quiet
        -n nthreads     use nthreads threads
        --              end option parsing

Placeholders:
    You can use {} or {i} with i a non-negative integer to refer
    to placeholders:
        - ls {}
        - cat {1} {0}
    BASH-style replacements are supported:
        - cat {} > {0/e/o}      # replace the first 'e' with 'o'
        - cat {} > {0//e/o}     # replace all 'e' with 'o'
        - cat {} > {0/%e/o}     # replace the final character
                                # with 'o' if it is 'e'

Example usage:
    The current directory contains story1, story1.part2, story2 and story2.part2.
    Running the following command will result in the files *.part2 being appended
    to the other files and being removed:
            bin/worker 'cat {} >> {} && rm {0}' '*.part2' 'story{1,2}'

    The current directory contains story1.part1, story1.part2, story2.part1 and
    story2.part2. To create files story1 and story2 containing the entire stories:
            bin/worker 'cat {} {0/%1/2} > {0/%.part1/}' \*.part1
```

Run ```bin/worker``` to get an updated usage statement.  

## License

This program is released under a modified MIT license. For the license, see LICENSE.md.
