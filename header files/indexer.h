#pragma once
#include <unordered_map>
#include <vector>
#include <string>
using namespace std;
class Indexer {
public:
    void indexDocument(int docID, const string& content);
    unordered_map<string, unordered_map<int, int>> getIndex();
    private:
    unordered_map<string, unordered_map<int, int>> invertedIndex;
};
