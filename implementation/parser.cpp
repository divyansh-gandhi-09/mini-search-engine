#include "parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
using namespace std;
string Parser::readFile(const string& filepath) {
    ifstream file(filepath);
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
vector<string> Parser::tokenize(const string& text) {
    vector<string> tokens;
    string word;
    for (char ch : text) {
        if (isalnum(ch)) {
            word += tolower(ch);
        } else if (!word.empty()) {
            tokens.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) tokens.push_back(word);
    return tokens;
}
