#include "parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

using namespace std;

string Parser::readFile(const string& filepath) {
    ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    
    if (file.bad()) {
        throw std::runtime_error("Error reading file: " + filepath);
    }
    
    return buffer.str();
}

vector<string> Parser::tokenize(const string& text) {
    vector<string> tokens;
    tokens.reserve(text.size() / 5);
    
    string word;
    word.reserve(50); // Pre-allocate for typical word length
    
    for (unsigned char ch : text) { // Use unsigned char for proper tolower
        if (isalnum(ch)) {
            // Use standard tolower - consistent and safe
            word += static_cast<char>(std::tolower(ch));
        } else if (!word.empty()) {
            tokens.push_back(std::move(word));
            word.clear();
            
        }
    }
    
    if (!word.empty()) {
        tokens.push_back(std::move(word));
    }
    
    return tokens;
}