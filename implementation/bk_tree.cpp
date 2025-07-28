#include "bk_tree.h"
#include <algorithm>
#include <queue>
#include <cmath>
using namespace std;
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
void BKTree::insert(const string& word) {
    if (!root) {
        root = new Node(word);
        return;
    }
    Node* curr = root;
    while (true) {
        int dist = levenshtein(word, curr->word);
        if (dist == 0) return; // already exists
        if (!curr->children[dist]) {
            curr->children[dist] = new Node(word);
            return;
        }
        curr = curr->children[dist];
    }
}
vector<string> BKTree::search(const string& target, int maxDistance) {
    vector<string> result;
    if (!root) return result;
    queue<Node*> q;
    q.push(root);

    while (!q.empty()) {
        Node* node = q.front(); q.pop();
        int dist = levenshtein(target, node->word);
        if (dist <= maxDistance) result.push_back(node->word);

        for (auto& [childDist, childNode] : node->children) {
            if (childDist >= dist - maxDistance && childDist <= dist + maxDistance)
                q.push(childNode);
        }
    }
    return result;
}
