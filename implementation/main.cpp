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
namespace fs = std::filesystem;
int main() {
    std::cout << "------ Mini Search Engine in C++ ------\n";
    std::cout << "Indexing documents from './data/'...\n";
    Indexer indexer;
    Trie autoComplete;
    BKTree typoCorrector;
    std::unordered_map<int, std::string> docIdToPath;
    int docID = 0;
    for (const auto& entry : fs::directory_iterator("./data")) {
        if (entry.is_regular_file()) {
            std::string path = entry.path().string();
            std::string content = Parser::readFile(path);
            indexer.indexDocument(docID, content);
            std::vector<std::string> tokens = Parser::tokenize(content);
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
    std::cout << "Indexing complete. " << docID << " documents indexed.\n";
    std::string query;
    while (true) {
        std::cout << "\n-> Enter search query (or 'exit'): ";
        std::getline(std::cin, query);
        if (query == "exit") break;

        // Autocomplete suggestions
        if (query.size() > 1) {
            std::cout << "-> Suggestions: ";
            auto suggestions = autoComplete.suggest(query);
            for (int i = 0; i < std::min(5, (int)suggestions.size()); ++i) {
                std::cout << suggestions[i] << " ";
            }
            std::cout << "\n";
        }
        // Typo correction
        auto corrections = typoCorrector.search(query, 2);
        if (!corrections.empty()) {
            std::cout << "-> Did you mean: ";
            for (int i = 0; i < std::min(3, (int)corrections.size()); ++i) {
                std::cout << corrections[i] << " ";
            }
            std::cout << "\n";
        }
        // Search and rank using SearchEngine
        auto ranked = search.search(query, Ranker());
        if (ranked.empty()) {
            std::cout << "-> No results found.\n";
        } else {
            std::cout << " -> Results:\n";
            for (const auto& [id, score] : ranked) {
                std::cout << " - " << docIdToPath[id] << " (score: " << score << ")\n";
            }
        }
    }
    std::cout << "Exiting...\n";
    return 0;
}

