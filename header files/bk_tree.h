#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class BKTree {
public:
    BKTree() : root(nullptr) {}
    ~BKTree();

    void insert(const std::string& word);
    std::vector<std::string> search(const std::string& target, int maxDist);

    // lazy delete
    void markDeleted(const std::string& word);

    void clear();

private:
    struct Node {
        std::string word;
        bool deleted = false;
        std::unordered_map<int, Node*> children;
        Node(const std::string& w) : word(w) {}
    };

    Node* root = nullptr;

    int levenshtein(const std::string& a, const std::string& b);
    void freeNode(Node* node);
};
