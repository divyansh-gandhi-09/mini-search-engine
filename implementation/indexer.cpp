#include "indexer.h"
#include "parser.h"
void Indexer::indexDocument(int docID, const std::string& content) {
    std::vector<std::string> words = Parser::tokenize(content);
    for (const std::string& word : words) {
        invertedIndex[word][docID]++;
    }
}
std::unordered_map<std::string, std::unordered_map<int, int>> Indexer::getIndex() {
    return invertedIndex;
}
