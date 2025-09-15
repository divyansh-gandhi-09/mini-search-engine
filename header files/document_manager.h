#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <memory>
#include "indexer.h"
#include "search.h"
#include "trie.h"
#include "bk_tree.h"

class DocumentManager {
private:
    std::unordered_map<int, std::vector<std::string>> docTokens;
    std::unordered_map<int, std::string> docIdToContent;
    std::unordered_map<int, std::string> docIdToPath;
    std::unordered_map<int, std::string> docIdToRel;
    std::unordered_map<int, std::string> docMeta;
    std::unordered_map<std::string, int> vocabCount;
    
    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unique_ptr<SearchEngine> engine;
    int docID;

public:
    DocumentManager();
    
    // Initialization
    bool initialize();
    void buildFreshIndex();
    void updateExistingIndex();
    void rebuildSearchStructures();
    
    // Document operations
    int uploadDocument(const std::string& filename, const std::string& content);
    bool editDocument(int id, const std::string& newContent);
    bool deleteDocument(int id);
    
    // Query operations
    std::vector<std::string> getSuggestions(const std::string& prefix);
    std::vector<std::string> getCorrections(const std::string& word, int maxResults = 3);
    std::vector<std::pair<int, double>> search(const std::string& query) const;
    
    // Getters
    const std::unordered_map<int, std::string>& getDocIdToContent() const { return docIdToContent; }
    const std::unordered_map<int, std::string>& getDocIdToPath() const { return docIdToPath; }
    const std::unordered_map<int, std::string>& getDocIdToRel() const { return docIdToRel; }
    const std::unordered_map<int, std::vector<std::string>>& getDocTokens() const { return docTokens; }
    const std::unordered_map<int, std::string>& getDocMeta() const { return docMeta; }
    const std::unordered_map<std::string, int>& getVocabCount() const { return vocabCount; }
    const Indexer& getIndexer() const { return indexer; }
    int getDocID() const { return docID; }
    
    // For persistence
    void updateFromPersistence(
        const std::unordered_map<int, std::string>& loadedDocIdToPath,
        const std::unordered_map<int, std::string>& loadedDocIdToRel,
        const std::unordered_map<int, std::vector<std::string>>& loadedDocTokens,
        const std::unordered_map<int, std::string>& loadedDocMeta,
        const std::unordered_map<std::string, int>& loadedVocabCount,
        int loadedDocID
    );
    
    void saveIndex() const;
};