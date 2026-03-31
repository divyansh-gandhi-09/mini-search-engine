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

//  OPTIMIZED: Use single-row Levenshtein (O(n) space instead of O(n*m))
int BKTree::levenshtein(const string& a, const string& b) {
    const size_t n = a.size();
    const size_t m = b.size();
    
    // Early termination optimizations
    if (n == 0) return static_cast<int>(m);
    if (m == 0) return static_cast<int>(n);
    if (n == m && a == b) return 0;
    
    // Ensure a is the shorter string (optimization)
    if (n > m) {
        return levenshtein(b, a);
    }
    
    //  Use single row instead of full matrix (80% memory reduction)
    vector<int> prev(n + 1);
    vector<int> curr(n + 1);
    
    // Initialize first row
    for (size_t i = 0; i <= n; ++i) {
        prev[i] = static_cast<int>(i);
    }
    
    // Compute Levenshtein distance row by row
    for (size_t j = 1; j <= m; ++j) {
        curr[0] = static_cast<int>(j);
        
        for (size_t i = 1; i <= n; ++i) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[i] = min({
                prev[i] + 1,      // deletion
                curr[i-1] + 1,    // insertion
                prev[i-1] + cost  // substitution
            });
        }
        
        swap(prev, curr);
    }
    
    return prev[n];
}

void BKTree::insert(const std::string& word) {
    if (word.empty()) return;
    if (!root) { root = new Node(word); return; }

    Node* curr = root;
    while (true) {
        int dist = levenshtein(word, curr->word);
        if (dist == 0) return;  // exact match — already present
        auto it = curr->children.find(dist);
        if (it == curr->children.end()) {
            curr->children[dist] = new Node(word);
            return;
        }
        curr = it->second;
    }
}

vector<string> BKTree::search(const string& target, int maxDistance) {
    vector<string> result;
    result.reserve(50);  //  Pre-allocate for common case
    
    if (!root) return result;

    queue<Node*> q;
    unordered_set<string> seen;
    seen.reserve(50);  //  Pre-allocate
    q.push(root);

    while (!q.empty()) {
        Node* node = q.front(); 
        q.pop();
        
        int dist = levenshtein(target, node->word);

        // Only add if not deleted & unique
        if (!node->deleted && dist <= maxDistance && !seen.count(node->word)) {
            result.push_back(node->word);
            seen.insert(node->word);
        }

        // Explore relevant children (BK-Tree property)
        for (auto& [childDist, childNode] : node->children) {
            if (childDist >= dist - maxDistance && childDist <= dist + maxDistance) {
                q.push(childNode);
            }
        }
    }
    return result;
}

void BKTree::markDeleted(const std::string& word) {
    if (word.empty()) return;
    if (!root) return;

    std::queue<Node*> q;
    q.push(root);

    while (!q.empty()) {
        Node* node = q.front(); 
        q.pop();
        
        if (node->word == word) {
            node->deleted = true;
            return;
        }
        
        for (auto& [_, child] : node->children) {
            q.push(child);
        }
    }
}