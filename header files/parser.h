#pragma once
#include <vector>
#include <string>
class Parser {
public:
    static std::vector<std::string> tokenize(const std::string& text);
    static std::string readFile(const std::string& filepath);
};
