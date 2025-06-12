#include "Utils.hpp"
#include <sstream>

std::vector<std::string> ft_split(const std::string& s, char delimiter) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;

    while (std::getline(iss, token, delimiter)) {
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}
