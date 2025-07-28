#include "indexer.h"
#include "parser.h"
using namespace std;
void Indexer::indexDocument(int docID, const string& content) {
    vector<string> words = Parser::tokenize(content);
    for (const string& word : words) {
        invertedIndex[word][docID]++;
    }
}
unordered_map<string, unordered_map<int, int>> Indexer::getIndex() {
    return invertedIndex;
}
