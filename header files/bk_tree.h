#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class BKTree {
public:
    void insert(const std::string& word);
    std::vector<std::string> search(const std::string& target, int maxDistance);
private:
    struct Node {
        std::string word;
        std::unordered_map<int, Node*> children;
        Node(const std::string& w) : word(w) {}
    };
    Node* root = nullptr;
    int levenshtein(const std::string& a, const std::string& b);
};
