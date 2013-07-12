#include "command.hpp"
#include "api.hpp"
#include <cstring>
#include <cstdlib>
#include <sstream>

#include <boost/regex.hpp>

/*
 * {
 *  (
 *    ([0-9]+)
 *    (
 *      /([%/]?)([^/]+)/(.+)
 *    )?
 *  )?
 * }
 */
#define PLACEHOLDER_REGEX         "" \
    "[{]" \
        "(" \
            "([0-9]+)" \
            "(" \
                "/([%/]?)([^/]*)/([^/}]*)" \
            ")?" \
        ")?" \
    "[}]"
#define REG_IDX_GROUP_NUMBER      2
#define REG_IDX_GROUP_REPLACE     3
#define REG_IDX_GROUP_REP_TYP     4
#define REG_IDX_GROUP_REP_OLD     5
#define REG_IDX_GROUP_REP_NEW     6



using namespace std;

namespace worker {

    namespace exception {

        invalid_argument_count::invalid_argument_count(uint expected, uint received) {
            stringstream s;
            if (received > expected) {
                s << "received too many arguments: ";
            } else {
                s << "received not enough arguments: ";
            }
            s << received << " instead of the expected " << expected;

            const string st = s.str();
            str = new char[st.length() + 1];
            memcpy(str, st.c_str(), st.length());
            str[st.length()] = '\0';
        }

        negative_index::negative_index(int index) {
            string s = "negative index not allowed: ";
            s += itoa(index);

            str = new char[s.length() + 1];
            memcpy(str, s.c_str(), s.length());
            str[s.length()] = '\0';
        }

        invalid_placeholder::invalid_placeholder(int index) {
            string s = "invalid placeholder index: ";
            s += itoa(index);

            str = new char[s.length() + 1];
            memcpy(str, s.c_str(), s.length());
            str[s.length()] = '\0';
        }

    }

    namespace impl {

        Replacement::Replacement(char typeIdentifier, const std::string &from, const std::string &to)
            : super_t((typeIdentifier == '%') ? REPLACE_END :
                    ((typeIdentifier == '/') ? REPLACE_ALL : REPLACE_FIRST),
                    from, to)
        {}

        string &Replacement::apply(string &str) const {
            switch(getType()) {
            case REPLACE_FIRST: {
                size_t idx = str.find(getOriginal());
                if (idx == string::npos) return str;
                size_t len = getOriginal().length();

                str.replace(idx, len, getReplacement());
                return str;
            }
            case REPLACE_ALL: {
                size_t offset = 0;
                string copy = str;
                while (true) {
                    size_t idx = copy.find(getOriginal());
                    if (idx == string::npos) return str;
                    size_t len = getOriginal().length();

                    str.replace(offset + idx, len, getReplacement());

                    offset += idx + len;
                    copy = str.c_str() + offset;
                }
            }
            case REPLACE_END: {
                size_t offset = str.length() - getOriginal().length();

                if (strcmp(str.c_str() + offset, getOriginal().c_str()) != 0) {
                    return str;
                }

                str.replace(offset, getOriginal().length(), getReplacement());
                return str;
            }
            default:
                return str;
            }
        }

        Placeholder::Placeholder(size_t offset, uint idx, size_t length, Replacement *replacement)
            : super_t(offset, idx, length, replacement)
        {}
    }

    typedef boost::regex regex_t;
    typedef boost::cmatch regmatch_t;

    static regex_t placeholderRegex(PLACEHOLDER_REGEX);

    uint getNbPlaceholders(const Command::indices_t &indices) {
        int result = -1;

        for (Command::indices_t::const_iterator current = indices.begin(), end = indices.end(); current != end; current++) {
            result = max(result, static_cast<int>(current->getIndex()));
        }

        return uint(result + 1);
    }

    Command::indices_t getPlaceholders(string &str) {
        Command::indices_t result;

        uint currentRef = 0;
        size_t stringOffset = 0;
        const char * const strStart = str.c_str();
        const char * strCurrent = strStart;

        regmatch_t match;

        Debug("Locating placeholders in string \"%s\"", strStart);

        while (true) {
            if (!boost::regex_search(strCurrent, match, placeholderRegex)) {
                if (!result.empty())
                    return result;
                
                Debug("No placeholders given, adding one");
                
                uint placeholderIdx = 0;
                size_t length = 2;
                size_t offset = str.length() + 1;
                str += " {}";
                
                result.push_back(Command::placeholder_t(offset, placeholderIdx, length, NULL));
                return result;
            }

            size_t offset = match.position();
            size_t length = match.length();

            uint placeholderIdx;
            Command::replacement_t *replacementPtr = NULL;
            if (length == 2)
                placeholderIdx = currentRef++;
            else {
                size_t idxOffset = match.position(REG_IDX_GROUP_NUMBER);
                size_t idxLength = match.length(REG_IDX_GROUP_NUMBER);

                char idx[idxLength + 1];
                memcpy(idx, strCurrent + idxOffset, idxLength);
                idx[idxLength] = '\0';

                int _pIdx = atoi(idx);
                if (_pIdx < 0) {
                    throw exception::negative_index(_pIdx);
                }
                placeholderIdx = uint(_pIdx);

                if (length != idxLength + 2) {
                    // replacement is a match!
                    size_t oldOffset = match.position(REG_IDX_GROUP_REP_OLD);
                    size_t oldLength = match.length(REG_IDX_GROUP_REP_OLD);

                    size_t newOffset = match.position(REG_IDX_GROUP_REP_NEW);
                    size_t newLength = match.length(REG_IDX_GROUP_REP_NEW);

                    char typeIdentifier = '\0';
                    if (match.length(REG_IDX_GROUP_REP_TYP))
                        typeIdentifier = *(strCurrent + match.position(REG_IDX_GROUP_REP_TYP));

                    char oldPart[oldLength + 1];
                    char newPart[newLength + 1];

                    memcpy(oldPart, strCurrent + oldOffset, oldLength);
                    oldPart[oldLength] = '\0';

                    memcpy(newPart, strCurrent + newOffset, newLength);
                    newPart[newLength] = '\0';

                    Debug("Command has replacement %s->%s", oldPart, newPart);
                    replacementPtr = new Command::replacement_t(typeIdentifier, oldPart, newPart);
                }
            }

            Debug("Placeholder %.*s references placeholder %u", length, strCurrent + offset, placeholderIdx);

            result.push_back(Command::placeholder_t(stringOffset + offset, placeholderIdx, length, replacementPtr));

            stringOffset += offset + length;
            strCurrent += offset + length;
        }
    }

    Command::Command(const string &c)
        : command(c), indices(getPlaceholders(const_cast<string &>(command)))
    {
        Debug("Created command for string \"%s\" with %u placeholder references", command.c_str(), indices.size());
        nbPlaceholders = ::worker::getNbPlaceholders(indices);
    }

    string Command::fillArguments(const arguments_t &arguments) const {
        if (arguments.size() != nbPlaceholders)
            throw exception::invalid_argument_count(nbPlaceholders, arguments.size());

        string command = this->command;

        for (indices_t::const_reverse_iterator current = indices.rbegin(), end = indices.rend(); current != end; current++) {
            size_t strIdx = current->getOffset();
            size_t strLen = current->getLength();

            uint placeholderId = current->getIndex();

            const replacement_t *replacement = current->getReplacement();
            string argument = arguments[placeholderId];

            if (replacement != NULL) {
                replacement->apply(argument);
            }

            Debug("running command.replace(%u, %u, \"%s\")", strIdx, strLen, argument.c_str());
            command.replace(strIdx, strLen, argument);
        }

        Debug("After filling in arguments \"%s\" becomes \"%s\".", this->command.c_str(), command.c_str());

        return command;
    }
}
