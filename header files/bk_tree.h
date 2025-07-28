#pragma once
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
class BKTree {
public:
    void insert(const string& word);
    vector<string> search(const string& target, int maxDistance);
private:
    struct Node {
        string word;
        unordered_map<int, Node*> children;
        Node(const string& w) : word(w) {}
    };
    Node* root = nullptr;
    int levenshtein(const string& a, const string& b);
};
