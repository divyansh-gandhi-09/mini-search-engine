#pragma once
#include <unordered_map>
#include <vector>
#include <string>
using namespace std;
class Trie {
private:
    struct Node {
        bool isWord = false;
        unordered_map<char, Node*> children;
    };

    Node* root;
    void dfs(Node* node, string& prefix, vector<string>& result);
    public:
    Trie();
    void insert(const string& word);
    vector<string> suggest(const string& prefix);
};
