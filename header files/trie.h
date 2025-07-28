#pragma once
#include <unordered_map>
#include <vector>
#include <string>
class Trie {
private:
    struct Node {
        bool isWord = false;
        std::unordered_map<char, Node*> children;
    };

    Node* root;
    void dfs(Node* node, std::string& prefix, std::vector<std::string>& result);
    public:
    Trie();
    void insert(const std::string& word);
    std::vector<std::string> suggest(const std::string& prefix);
};
