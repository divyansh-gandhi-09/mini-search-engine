#include "search.h"
#include "parser.h"
#include "ranker.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <unordered_map>
SearchEngine::SearchEngine(const std::unordered_map<std::string, std::unordered_map<int, int>>& index, int totalDocs)
    : invertedIndex(index), totalDocuments(totalDocs) {}
std::vector<std::pair<int, double>> SearchEngine::search(const std::string& query, const Ranker& ranker) {
    std::stringstream ss(query);
    std::string word;
    std::vector<std::string> terms;
    bool isAnd = false, isOr = false;
    // Parse and lowercase query
    while (ss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (word == "and") isAnd = true;
        else if (word == "or") isOr = true;
        else terms.push_back(word);
    }
    // Store document scores
    std::unordered_map<int, double> docScores;
    for (const std::string& term : terms) {
        auto ranked = ranker.rank(term, invertedIndex, totalDocuments);
        for (auto& [docID, score] : ranked) {
            if (isOr) {
                docScores[docID] += score;
            } else if (isAnd) {
                // AND: only keep if already present
                if (docScores.empty() || docScores.count(docID)) {
                    docScores[docID] += score;
                }
            } else {
                // Single term
                docScores[docID] += score;
            }
        }
    }

    // Convert to vector and sort by score
    std::vector<std::pair<int, double>> results(docScores.begin(), docScores.end());
    std::sort(results.begin(), results.end(), [](auto& a, auto& b) {
        return a.second > b.second;
    });

    return results;
}
