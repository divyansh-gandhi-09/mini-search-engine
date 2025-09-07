#pragma once
#include <vector>
#include <string>
using namespace std;

class Trie {
private:
    struct Node {
        bool isWord = false;
        Node* children[256] = {nullptr};  // full ASCII
    };

    Node* root;

    void dfs(Node* node, string& prefix, vector<string>& result);
    void freeNode(Node* node);
    bool removeHelper(Node* node, const string& word, int depth);

public:
    Trie();
    ~Trie();

    void insert(const string& word);
    vector<string> suggest(const string& prefix);
    void clear();
    bool remove(const string& word);
};
