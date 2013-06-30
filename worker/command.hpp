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

    namespace impl {

        typedef enum { REPLACE_FIRST, REPLACE_ALL, REPLACE_END } ReplacementType;

        struct Replacement : public std::tuple<ReplacementType, std::string, std::string> {
        private:
            typedef std::tuple<ReplacementType, std::string, std::string> super_t;

        public:
            Replacement(char typeIdentifier, const std::string &from, const std::string &to);

            inline ReplacementType getType() const {
                return std::get<0>(*this);
            }

            inline const std::string &getOriginal() const {
                return std::get<1>(*this);
            }

            inline const std::string &getReplacement() const {
                return std::get<2>(*this);
            }

            std::string &apply(std::string &str) const;
        };

        struct Placeholder : public std::tuple<size_t, uint, size_t, Replacement*> {
        private:
            typedef std::tuple<size_t, uint, size_t, Replacement*> super_t;

        public:
            Placeholder(size_t offset, uint idx, size_t length, Replacement *replacement);

            inline size_t getOffset() const {
                return std::get<0>(*this);
            }

            inline uint getIndex() const {
                return std::get<1>(*this);
            }

            inline size_t getLength() const {
                return std::get<2>(*this);
            }

            inline const Replacement *getReplacement() const {
                return std::get<3>(*this);
            }
        };
    }

    class Command {
    public:
        typedef impl::Replacement replacement_t;
        typedef impl::Placeholder placeholder_t;
        typedef std::vector<placeholder_t> indices_t;
        typedef std::vector<std::string> arguments_t;

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