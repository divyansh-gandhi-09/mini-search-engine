#pragma once
#include <vector>
#include <string>
using namespace std;
class Parser {
public:
    static vector<string> tokenize(const string& text);
    static string readFile(const string& filepath);
};
