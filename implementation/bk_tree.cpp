#include "bk_tree.h"
#include <algorithm>
#include <queue>
#include <cmath>
#include <unordered_set>

using namespace std;
BKTree::~BKTree() {
    clear();
}

void BKTree::freeNode(Node* node) {
    if (!node) return;
    for (auto& [d, child] : node->children) {
        freeNode(child);
    }
    delete node;
}

void BKTree::clear() {
    freeNode(root);
    root = nullptr;
}
int BKTree::levenshtein(const string& a, const string& b) {
    int n = a.size(), m = b.size();
    vector<vector<int>> dp(n + 1, vector<int>(m + 1));
   
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (a[i-1] == b[j-1])
                dp[i][j] = dp[i-1][j-1];
            else
                dp[i][j] = 1 + min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
        }
    }
    return dp[n][m];
}
void BKTree::insert(const std::string& word) {
    if (word.empty()) return;
    if (!root) {
        root = new Node(word);
        return;
    }
    Node* curr = root;
    int dist = levenshtein(word, curr->word);
    
    while (true) {
        if (curr->word == word) return; // Check before descending
        
        if (!curr->children.count(dist)) {
            curr->children[dist] = new Node(word);
            return;
        }
        
        curr = curr->children[dist];
        dist = levenshtein(word, curr->word);
    }
}

vector<string> BKTree::search(const string& target, int maxDistance) {
    vector<string> result;
    if (!root) return result;

    queue<Node*> q;
    unordered_set<string> seen; //  duplicates
    q.push(root);

    while (!q.empty()) {
        Node* node = q.front(); q.pop();
        int dist = levenshtein(target, node->word);

        //  only add if not deleted & unique
        if (!node->deleted && dist <= maxDistance && !seen.count(node->word)) {
            result.push_back(node->word);
            seen.insert(node->word);
        }

        // explore relevant children
        for (auto& [childDist, childNode] : node->children) {
            if (childDist >= dist - maxDistance && childDist <= dist + maxDistance) {
                q.push(childNode);
            }
        }
    }
    return result;
}
void BKTree::markDeleted(const std::string& word) {
    if (word.empty()) return; // safety check
    if (!root) return;

    std::queue<Node*> q;
    q.push(root);

    while (!q.empty()) {
        Node* node = q.front(); q.pop();
        if (node->word == word) {
            node->deleted = true; //  mark as deleted
            return;
        }
        for (auto& [_, child] : node->children) {
            q.push(child);
        }
    }
}