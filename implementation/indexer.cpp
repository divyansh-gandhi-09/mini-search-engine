#include "indexer.h"
#include "parser.h"
#include <vector>
#include <string>

void Indexer::indexDocument(int docID, const std::string& content) {
    std::vector<std::string> words = Parser::tokenize(content);
    for (const std::string& word : words) {
        invertedIndex[word][docID]++; // increment count
    }
}

void Indexer::indexDocumentFromTokens(int docID, const std::vector<std::string>& tokens) {
    for (const std::string& word : tokens) {
        invertedIndex[word][docID]++; // increment count
    }
}

std::unordered_map<std::string, std::unordered_map<int,int>> Indexer::getIndex() const {
    return invertedIndex;
}

void Indexer::setIndex(const std::unordered_map<std::string, std::unordered_map<int,int>>& idx) {
    invertedIndex = idx; // replace whole index
}

void Indexer::removeDocument(int docId, const std::vector<std::string>& tokens) {
    for (const auto& term : tokens) {
        auto it = invertedIndex.find(term);
        if (it != invertedIndex.end()) {
            it->second.erase(docId); // remove posting
            if (it->second.empty()) {
                invertedIndex.erase(it); // clean up empty entry
            }
        }
    }
}
