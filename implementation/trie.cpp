#include "trie.h"
Trie::Trie() {
    root = new Node();
}
void Trie::insert(const std::string& word) {
    Node* curr = root;
    for (char ch : word) {
        if (!curr->children[ch]) curr->children[ch] = new Node();
        curr = curr->children[ch];
    }
    curr->isWord = true;
}
void Trie::dfs(Node* node, std::string& prefix, std::vector<std::string>& result) {
    if (node->isWord) result.push_back(prefix);
    for (auto& [ch, child] : node->children) {
        prefix.push_back(ch);
        dfs(child, prefix, result);
        prefix.pop_back();
    }
}
std::vector<std::string> Trie::suggest(const std::string& prefix) {
    Node* curr = root;
    for (char ch : prefix) {
        if (!curr->children.count(ch)) return {};
        curr = curr->children[ch];
    }
    std::vector<std::string> result;
    std::string temp = prefix;
    dfs(curr, temp, result);
    return result;
}
