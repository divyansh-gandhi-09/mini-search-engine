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
    std::unordered_map<int, std::string> docIdToFolder;
    bool batchMode = false;  
    bool silentMode = false;
    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unique_ptr<SearchEngine> engine;
    int docID;
    std::mutex docIdMutex;

    std::string extractFolderFromPath(const std::string& filepath);

public:
    DocumentManager();
    bool initialize();
    void buildFreshIndex();
    void updateExistingIndex();
    void rebuildSearchStructures(bool clearContent = true);  //  Added parameter
    void setSilentMode(bool enabled) { silentMode = enabled; }
    int uploadDocument(const std::string& filename, const std::string& content, const std::string& folder = "");
    bool editDocument(int id, const std::string& newContent);
    bool deleteDocument(int id);

    std::vector<std::string> getSuggestions(const std::string& prefix);
    std::vector<std::string> getCorrections(const std::string& word, int maxResults = 3);
    std::vector<std::pair<int, double>> search(const std::string& query) const;

    const std::unordered_map<int, std::string>& getDocIdToContent() const { return docIdToContent; }
    const std::unordered_map<int, std::string>& getDocIdToPath() const { return docIdToPath; }
    const std::unordered_map<int, std::string>& getDocIdToRel() const { return docIdToRel; }
    const std::unordered_map<int, std::vector<std::string>>& getDocTokens() const { return docTokens; }
    const std::unordered_map<int, std::string>& getDocMeta() const { return docMeta; }
    const std::unordered_map<std::string, int>& getVocabCount() const { return vocabCount; }
    const std::unordered_map<int, std::string>& getDocIdToFolder() const { return docIdToFolder; }
    std::string getFolder(int docId) const;
    std::string getDocumentContent(int docId);
    const Indexer& getIndexer() const { return indexer; }
    int getDocID() const { return docID; }
    void setBatchMode(bool enabled) { batchMode = enabled; }
    void finalizeBatch();

    void updateFromPersistence(
        const std::unordered_map<int, std::string>& loadedDocIdToPath,
        const std::unordered_map<int, std::string>& loadedDocIdToRel,
        const std::unordered_map<int, std::vector<std::string>>& loadedDocTokens,
        const std::unordered_map<int, std::string>& loadedDocMeta,
        const std::unordered_map<std::string, int>& loadedVocabCount,
        const std::unordered_map<int, std::string>& loadedDocIdToFolder,
        int loadedDocID
    );

    void saveIndex() const;
};