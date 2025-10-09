#include "trie.h"
using namespace std;

Trie::Trie() {
    root = new Node();
}

Trie::~Trie() {
    clear();
    delete root;
    root = nullptr;
}

void Trie::freeNode(Node* node) {
    if (!node) return;
    
    for (int i = 0; i < 256; i++) {
        if (node->children[i]) {
            freeNode(node->children[i]);
            node->children[i] = nullptr;
        }
    }
    delete node;
}

void Trie::clear() {
    // Free all children but NOT root itself
    for (int i = 0; i < 256; i++) {
        if (root->children[i]) {
            freeNode(root->children[i]);
            root->children[i] = nullptr;
        }
    }
    root->isWord = false;
}

void Trie::insert(const std::string& word) {
    if (word.empty()) return; // safety check
    Node* node = root;
    for (unsigned char c : word) {
        if (!node->children[c]) node->children[c] = new Node();
        node = node->children[c];
    }
    node->isWord = true;
}

bool Trie::remove(const std::string& word) {
    if (word.empty()) return false; // safety check
    return removeHelper(root, word, 0);
}

void Trie::dfs(Node* node, string& prefix, vector<string>& result) {
    const size_t limit = 20;
    if (result.size() >= limit) return;

    if (node->isWord) result.push_back(prefix);

    for (int i = 0; i < 256; i++) {
        if (node->children[i]) {
            prefix.push_back((char)i);
            dfs(node->children[i], prefix, result);
            prefix.pop_back();
            if (result.size() >= limit) return;
        }
    }
}

vector<string> Trie::suggest(const string& prefix) {
    Node* curr = root;
    for (unsigned char ch : prefix) {
        if (!curr->children[ch]) return {};
        curr = curr->children[ch];
    }

    vector<string> result;
    string temp = prefix;
    dfs(curr, temp, result);
    return result;
}

bool Trie::removeHelper(Node* node, const string& word, int depth) {
    if (!node) return false;

    if (depth == word.size()) {
        if (!node->isWord) return false;
        node->isWord = false;

        for (int i = 0; i < 256; i++) {
            if (node->children[i]) return false;
        }
        return true;
    }

    unsigned char ch = word[depth];
    if (!node->children[ch]) return false;

    bool shouldDelete = removeHelper(node->children[ch], word, depth + 1);
    if (shouldDelete) {
        delete node->children[ch];
        node->children[ch] = nullptr;

        if (!node->isWord) {
            for (int i = 0; i < 256; i++) {
                if (node->children[i]) return false;
            }
            return true;
        }
    }
    return false;
}