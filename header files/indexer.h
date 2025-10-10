#pragma once
#include <unordered_map>
#include <string>
#include <vector>

class Indexer {
public:
    void indexDocument(int docID, const std::string& content);
    void indexDocumentFromTokens(int docID, const std::vector<std::string>& tokens);
    void removeDocument(int docID, const std::vector<std::string>& tokens);
    const std::unordered_map<std::string, std::unordered_map<int,int>>& getIndex() const;
    void setIndex(const std::unordered_map<std::string, std::unordered_map<int,int>>& idx);
    void clear() { invertedIndex.clear(); }

private:
    std::unordered_map<std::string, std::unordered_map<int,int>> invertedIndex;
};
