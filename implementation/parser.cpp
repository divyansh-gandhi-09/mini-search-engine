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
    tokens.reserve(text.size()/5); //  avoid frequent reallocations
    string word;
    for (char ch : text) {
        if (isalnum(ch)) word += (ch | 0x20); // faster lowercase for ASCII
        else if (!word.empty()) {
            tokens.push_back(move(word));
            word.clear();
        }
    }
    if (!word.empty()) tokens.push_back(move(word));
    return tokens;
}

