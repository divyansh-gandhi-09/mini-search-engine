#include "parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
std::string Parser::readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
std::vector<std::string> Parser::tokenize(const std::string& text) {
    std::vector<std::string> tokens;
    std::string word;
    for (char ch : text) {
        if (std::isalnum(ch)) {
            word += std::tolower(ch);
        } else if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) tokens.push_back(word);
    return tokens;
}
