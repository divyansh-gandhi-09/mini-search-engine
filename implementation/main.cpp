#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include "parser.h"
#include "indexer.h"
#include "search.h"
#include "ranker.h"
#include "trie.h"
#include "bk_tree.h"
using namespace std;
namespace fs = std::filesystem;
int main() {
    cout << "\n------ Mini Search Engine in C++ ------\n";
    cout << "Indexing documents from './data/'...\n";
    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    unordered_map<int, string> docIdToPath;
    int docID = 0;
    for (const auto& entry : fs::directory_iterator("./data")) {
        if (entry.is_regular_file()) {
            string path = entry.path().string();
            string content = Parser::readFile(path);
            indexer.indexDocument(docID, content);
            vector<string> tokens = Parser::tokenize(content);
            for (const auto& word : tokens) {
                autoComplete.insert(word);
                typoCorrector.insert(word);
            }
            docIdToPath[docID] = path;
            ++docID;
        }
    }
    auto index = indexer.getIndex();
    SearchEngine search(index, docID);  // Fixed constructor
    cout << "Indexing complete. " << docID << " documents indexed.\n";
    string query;
    while (true) {
        cout << "\n-> Enter search query (or 'exit'): ";
        getline(cin, query);
        if (query == "exit") break;

        // Autocomplete suggestions
        if (query.size() > 1) {
            cout << "-> Suggestions: ";
            auto suggestions = autoComplete.suggest(query);
            for (int i = 0; i < min(5, (int)suggestions.size()); ++i) {
                cout << suggestions[i] << " ";
            }
            cout << "\n";
        }
        // Typo correction
        auto corrections = typoCorrector.search(query, 2);
        if (!corrections.empty()) {
            cout << "-> Did you mean: ";
            for (int i = 0; i < min(3, (int)corrections.size()); ++i) {
                cout << corrections[i] << " ";
            }
            cout << "\n";
        }
        // Search and rank using SearchEngine
        auto ranked = search.search(query, Ranker());
        if (ranked.empty()) {
            cout << "-> No results found.\n";
        } else {
            cout << "-> Results:\n";
            for (const auto& [id, score] : ranked) {
                cout << " - " << docIdToPath[id] << " (score: " << score << ")\n";
            }
        }
    }
    cout << "Exiting...\n";
    return 0;
}

