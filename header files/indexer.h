#pragma once
#include <unordered_map>
#include <vector>
#include <string>
class Indexer {
public:
    void indexDocument(int docID, const std::string& content);
    std::unordered_map<std::string, std::unordered_map<int, int>> getIndex();
    private:
    std::unordered_map<std::string, std::unordered_map<int, int>> invertedIndex;
};
