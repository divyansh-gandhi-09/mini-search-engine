#include "search.h"
#include "parser.h"
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <cctype>


SearchEngine::SearchEngine(
    const std::unordered_map<std::string, std::unordered_map<int,int>>& index,
    int totalDocs)
    : invertedIndex(index), totalDocuments(totalDocs) {}

// ---------------- Search ----------------
std::vector<std::pair<int,double>> SearchEngine::search(const std::string& query, const Ranker& ranker) {
    std::vector<std::string> terms = Parser::tokenize(query);
    std::unordered_map<int,double> scores;
    
    // Check if query contains "AND" - case insensitive
    bool isAndQuery = false;
    std::vector<std::string> actualTerms;
    
    for (size_t i = 0; i < terms.size(); ++i) {
        std::string term = terms[i];
        // Convert to lowercase for comparison
        std::transform(term.begin(), term.end(), term.begin(), ::tolower);
        
        if (term == "and") {
            isAndQuery = true;
        } else {
            actualTerms.push_back(terms[i]); // Keep original case
        }
    }

    if (isAndQuery && actualTerms.size() >= 2) {
        // Use AND operation - only documents containing ALL terms
        std::unordered_set<int> validDocs = andOperation(actualTerms);
        
        if (validDocs.empty()) return {}; // No documents contain all terms
        
        // Calculate scores only for documents that contain ALL terms
        for (const auto& term : actualTerms) {
            auto it = invertedIndex.find(term);
            if (it == invertedIndex.end()) continue;

            const auto& docFreqMap = it->second;
            int df = static_cast<int>(docFreqMap.size());

            for (const auto& [docId, freq] : docFreqMap) {
                // Only score documents that passed the AND filter
                if (validDocs.count(docId)) {
                    double s = ranker.score(term, freq, totalDocuments, df);
                    scores[docId] += s;
                }
            }
        }
    } else {
        // Regular OR operation - documents containing ANY term
        for (const auto& term : actualTerms) {
            auto it = invertedIndex.find(term);
            if (it == invertedIndex.end()) continue;

            const auto& docFreqMap = it->second;
            int df = static_cast<int>(docFreqMap.size());

            for (const auto& [docId, freq] : docFreqMap) {
                double s = ranker.score(term, freq, totalDocuments, df);
                scores[docId] += s;
            }
        }
    }

    std::vector<std::pair<int,double>> results(scores.begin(), scores.end());
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });
    return results;
}

// ---------------- AND operation ----------------
std::unordered_set<int> SearchEngine::andOperation(const std::vector<std::string>& terms) {
    if (terms.empty()) return {};
    
    auto it = invertedIndex.find(terms[0]);
    if (it == invertedIndex.end()) return {};

    std::unordered_set<int> resultSet;
    for (const auto& [docId, freq] : it->second) {
        resultSet.insert(docId);
    }

    for (size_t i = 1; i < terms.size(); ++i) {
        auto it2 = invertedIndex.find(terms[i]);
        if (it2 == invertedIndex.end()) return {}; // Empty result if any term not found
        
        std::unordered_set<int> currentSet;
        for (const auto& [docId, freq] : it2->second) {
            currentSet.insert(docId);
        }

        // Intersect: keep only documents that appear in both sets
        std::unordered_set<int> newSet;
        for (int docId : resultSet) {
            if (currentSet.count(docId)) {
                newSet.insert(docId);
            }
        }
        resultSet = std::move(newSet);
        
        // If resultSet becomes empty, no need to continue
        if (resultSet.empty()) {
            break;
        }
    }

    return resultSet;
}

// ---------------- OR operation ----------------
std::unordered_set<int> SearchEngine::orOperation(const std::vector<std::string>& terms) {
    std::unordered_set<int> resultSet;
    
    for (const std::string& term : terms) {
        auto it = invertedIndex.find(term);
        if (it == invertedIndex.end()) continue; // Skip terms not in index
        
        for (const auto& [docId, freq] : it->second) {
            resultSet.insert(docId);
        }
    }
    
    return resultSet;
}