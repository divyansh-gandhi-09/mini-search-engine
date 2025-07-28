#include "search.h"
#include "parser.h"
#include "ranker.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <unordered_map>
using namespace std;
SearchEngine::SearchEngine(const unordered_map<string, unordered_map<int, int>>& index, int totalDocs)
    : invertedIndex(index), totalDocuments(totalDocs) {}
vector<pair<int, double>> SearchEngine::search(const string& query, const Ranker& ranker) {
    stringstream ss(query);
    string word;
    vector<string> terms;
    bool isAnd = false, isOr = false;
    // Parse and lowercase query
    while (ss >> word) {
        transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (word == "and") isAnd = true;
        else if (word == "or") isOr = true;
        else terms.push_back(word);
    }
    // Store document scores
    unordered_map<int, double> docScores;
    for (const string& term : terms) {
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
    vector<pair<int, double>> results(docScores.begin(), docScores.end());
    sort(results.begin(), results.end(), [](auto& a, auto& b) {
        return a.second > b.second;
    });

    return results;
}
