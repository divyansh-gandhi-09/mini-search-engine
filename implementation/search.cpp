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

    // Parse query
    while (ss >> word) {
        transform(word.begin(), word.end(), word.begin(), ::tolower);
        if (word == "and") isAnd = true;
        else if (word == "or") isOr = true;
        else terms.push_back(word);
    }

    unordered_map<int, double> docScores;

    if (isAnd) {
        // AND logic
        vector<unordered_set<int>> termDocSets;
        for (const string& term : terms) {
            unordered_set<int> docs;
            auto ranked = ranker.rank(term, invertedIndex, totalDocuments);
            for (auto& [docID, _] : ranked)
                docs.insert(docID);
            termDocSets.push_back(docs);
        }

        // Intersect all sets
        unordered_set<int> commonDocs = termDocSets[0];
        for (size_t i = 1; i < termDocSets.size(); ++i) {
            unordered_set<int> temp;
            for (int doc : commonDocs) {
                if (termDocSets[i].count(doc)) temp.insert(doc);
            }
            commonDocs = move(temp);
        }

        // Only score common docs
        for (const string& term : terms) {
            auto ranked = ranker.rank(term, invertedIndex, totalDocuments);
            for (auto& [docID, score] : ranked) {
                if (commonDocs.count(docID)) {
                    docScores[docID] += score;
                }
            }
        }
    } else {
        // OR or single term logic
        for (const string& term : terms) {
            auto ranked = ranker.rank(term, invertedIndex, totalDocuments);
            for (auto& [docID, score] : ranked) {
                docScores[docID] += score;
            }
        }
    }

    // Convert to sorted vector
    vector<pair<int, double>> results(docScores.begin(), docScores.end());
    sort(results.begin(), results.end(), [](auto& a, auto& b) {
        return a.second > b.second;
    });

    return results;
}

