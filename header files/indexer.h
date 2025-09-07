#pragma once
#include <unordered_map>
#include <string>
#include <vector>

class Indexer {
public:
    // Index a document (from raw content)
    void indexDocument(int docID, const std::string& content);

    // Index a document using already-tokenized words
    void indexDocumentFromTokens(int docID, const std::vector<std::string>& tokens);

    // Remove a document's postings (fast: only touches given tokens)
    void removeDocument(int docID, const std::vector<std::string>& tokens);

    // Getter / Setter for persistence
    std::unordered_map<std::string, std::unordered_map<int,int>> getIndex() const;
    void setIndex(const std::unordered_map<std::string, std::unordered_map<int,int>>& idx);

    // Clear entire index
    void clear() { invertedIndex.clear(); }

private:
    // term -> (docID -> count)
    std::unordered_map<std::string, std::unordered_map<int,int>> invertedIndex;
};
