#ifndef __WORKER_COMMAND_
#define __WORKER_COMMAND_

#include "api.hpp"
#include <string>
#include <exception>
#include <vector>
#include <tuple>

namespace worker {
    
    namespace exception {
    
        struct invalid_argument_count : public exception {
        public:
            invalid_argument_count(uint expected, uint received);
        };
        
        struct negative_index : public exception {
            negative_index(int index);
        };
        
        struct invalid_placeholder : public exception {
            invalid_placeholder(int index);
        };
    
    }

    class Command {
    public:
        typedef std::tuple<size_t, uint, size_t> placeholder_t; // index @ string, placeholder idx, placeholder length
        typedef std::vector<std::tuple<size_t, uint, size_t>> indices_t;
        typedef std::vector<std::string> arguments_t;
        
        static inline size_t getStringIndex(const placeholder_t &placeholder) {
            return std::get<0>(placeholder);
        }
        
        static inline uint getPlaceholderIndex(const placeholder_t &placeholder) {
            return std::get<1>(placeholder);
        }
        
        static inline size_t getPlaceholderLength(const placeholder_t &placeholder) {
            return std::get<2>(placeholder);
        }
        
    private:
        const std::string command;
        const indices_t indices;
        uint nbPlaceholders;
    
    public:
        Command(const std::string &command);
        
        inline uint getNbPlaceholders() const {
            return nbPlaceholders;
        }
        std::string fillArguments(const arguments_t &arguments) const;
    };

}

#endif // !defined(__WORKER_COMMAND_)