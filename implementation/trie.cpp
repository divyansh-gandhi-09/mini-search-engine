#include "trie.h"
using namespace std;
Trie::Trie() {
    root = new Node();
}
void Trie::insert(const string& word) {
    Node* curr = root;
    for (char ch : word) {
        if (!curr->children[ch]) curr->children[ch] = new Node();
        curr = curr->children[ch];
    }
    curr->isWord = true;
}
void Trie::dfs(Node* node, string& prefix, vector<string>& result) {
    if (node->isWord) result.push_back(prefix);
    for (auto& [ch, child] : node->children) {
        prefix.push_back(ch);
        dfs(child, prefix, result);
        prefix.pop_back();
    }
}
vector<string> Trie::suggest(const string& prefix) {
    Node* curr = root;
    for (char ch : prefix) {
        if (!curr->children.count(ch)) return {};
        curr = curr->children[ch];
    }
    vector<string> result;
    string temp = prefix;
    dfs(curr, temp, result);
    return result;
}
