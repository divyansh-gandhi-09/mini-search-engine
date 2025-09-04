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
// Private DFS with limit, signature matches header
void Trie::dfs(Node* node, string& prefix, vector<string>& result) {
    const size_t limit = 20;  // fixed number of suggestions

    if (result.size() >= limit) return;  // stop early if limit reached
    if (node->isWord) result.push_back(prefix);
    for (auto& [ch, child] : node->children) {
        prefix.push_back(ch);
        dfs(child, prefix, result);
        prefix.pop_back();
        if (result.size() >= limit) return;  // stop early
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
    dfs(curr, temp, result);  // limit handled inside dfs
    return result;
}
