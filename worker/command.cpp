#include "command.hpp"
#include "api.hpp"
#include <cstring>
#include <regex.h>
#include <cstdlib>
#include <sstream>

// old placeholder
//#define PLACEHOLDER_REGEX "{([0-9]*)}"

/*
 * {
 *  (
 *    ([0-9]+)
 *    (
 *      /%([^/]+)/(.+)
 *    )?
 *  )?
 * }
 */
#define PLACEHOLDER_REGEX         "{(([0-9]+)(/%([^/]+)/([^}]*))?)?}"
#define REG_NB_GROUPS             5
#define REG_IDX_GROUP_NUMBER      2
#define REG_IDX_GROUP_REPLACE     3
#define REG_IDX_GROUP_REP_OLD     4
#define REG_IDX_GROUP_REP_NEW     5

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
    
    static regex_t placeholder;
    static bool placeholderInitialised(false);
    
    /* _NOT_ thread safe */
    void initPlaceholder() {
        if (placeholderInitialised)
            return;
        
        int retval = regcomp(&placeholder, PLACEHOLDER_REGEX, REG_EXTENDED);
        
        if (retval != 0) {
            Fatal("regcomp returned error %d", retval);
        }
        
        placeholderInitialised = true;
    }
    
    uint getNbPlaceholders(const Command::indices_t &indices) {
        int result = -1;
        
        for (Command::indices_t::const_iterator current = indices.begin(), end = indices.end(); current != end; current++) {
            result = max(result, static_cast<int>(Command::getPlaceholderIndex(*current)));
        }
        
        return uint(result + 1);
    }
    
    Command::indices_t getPlaceholders(const string &str) {
        initPlaceholder();
        
        regmatch_t matches[REG_NB_GROUPS + 1];
        
        Command::indices_t result;
        
        int retval;
        uint currentRef;
        size_t stringOffset = 0;
        
        Debug("Locating placeholders in string \"%s\"", str.c_str());
        
        while (true) {
            if (0 != (retval = regexec(&placeholder, str.c_str() + stringOffset, REG_NB_GROUPS + 1, matches, 0))) {
                if (retval == REG_NOMATCH) {
                    Debug("Found a total of %u placeholders", result.size());
                    return result;
                }
            
                Fatal("regexec returned value %d", retval);
            }
            
            size_t offset = matches[0].rm_so;
            size_t length = size_t(matches[0].rm_eo - offset);
            
            uint placeholderIdx;
            Command::replacement_t *replacementPtr = NULL;
            if (length == 2)
                placeholderIdx = currentRef++;
            else {
                size_t idxOffset = matches[REG_IDX_GROUP_NUMBER].rm_so;
                size_t idxLength = size_t(matches[REG_IDX_GROUP_NUMBER].rm_eo - idxOffset);
                
                char idx[idxLength + 1];
                memcpy(idx, str.c_str() + idxOffset, idxLength);
                idx[idxLength] = '\0';
                
                int _pIdx = atoi(idx);
                if (_pIdx < 0) {
                    throw exception::negative_index(_pIdx);
                }
                placeholderIdx = uint(_pIdx);
                
                if (length != idxLength + 2) {
                    // replacement is a match!
                    size_t oldOffset = matches[REG_IDX_GROUP_REP_OLD].rm_so;
                    size_t oldLength = matches[REG_IDX_GROUP_REP_OLD].rm_eo - oldOffset;
                    
                    size_t newOffset = matches[REG_IDX_GROUP_REP_NEW].rm_so;
                    size_t newLength = matches[REG_IDX_GROUP_REP_NEW].rm_eo - newOffset;
                    
                    oldOffset += stringOffset;
                    newOffset += stringOffset;
                    
                    char oldPart[oldLength + 1];
                    char newPart[newLength + 1];
                    
                    memcpy(oldPart, str.c_str() + oldOffset, oldLength);
                    oldPart[oldLength] = '\0';
                    
                    memcpy(newPart, str.c_str() + newOffset, newLength);
                    newPart[newLength] = '\0';
                    
                    Debug("Command has replacement %s->%s", oldPart, newPart);
                    replacementPtr = new Command::replacement_t(oldPart, newPart);
                }
            }
            
            Debug("Placeholder %.*s references placeholder %u", length, str.c_str() + stringOffset + offset, placeholderIdx);
            
            result.push_back(Command::placeholder_t(stringOffset + offset, placeholderIdx, length, replacementPtr));
            
            stringOffset += offset + length;
        }
    }

    Command::Command(const string &command)
        : command(command), indices(getPlaceholders(command))
    {
        Debug("Created command for string \"%s\" with %u placeholder references", command.c_str(), indices.size());
        nbPlaceholders = ::worker::getNbPlaceholders(indices);
    }
    

    string Command::fillArguments(const arguments_t &arguments) const {
        if (arguments.size() != nbPlaceholders)
            throw exception::invalid_argument_count(nbPlaceholders, arguments.size());
        
        string command = this->command;
        
        for (indices_t::const_reverse_iterator current = indices.rbegin(), end = indices.rend(); current != end; current++) {
            size_t strIdx = getStringIndex(*current);
            size_t strLen = getPlaceholderLength(*current);
            
            uint placeholderId = getPlaceholderIndex(*current);
            
            replacement_t *replacement = getReplacement(*current);
            string argument = arguments[placeholderId];
            
            if (replacement != NULL) {
                string &oldPart = get<0>(*replacement);
                string &newPart = get<1>(*replacement);
                Debug("running replacement %s->%s on %s", oldPart.c_str(), newPart.c_str(), argument.c_str());
                
                size_t replIdx;
                if (string::npos != (replIdx = argument.rfind(oldPart))) {
                    argument.replace(replIdx, oldPart.length(), newPart);
                }
            }
            
            Debug("running command.replace(%u, %u, \"%s\")", strIdx, strLen, argument.c_str());
            command.replace(strIdx, strLen, argument);
        }
        
        Debug("After filling in arguments \"%s\" becomes \"%s\".", this->command.c_str(), command.c_str());
        
        return command;
    }
}