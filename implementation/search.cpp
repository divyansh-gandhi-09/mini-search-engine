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
    // Parser::tokenize already converts to lowercase!
    std::vector<std::string> terms = Parser::tokenize(query);
    std::unordered_map<int,double> scores;
    
    // Check if query contains "AND"
    bool isAndQuery = false;
    std::vector<std::string> actualTerms;
    
    // NO TOLOWER HERE - Parser already did it!
    for (const auto& term : terms) {
        if (term == "and") {
            isAndQuery = true;
        } else {
            actualTerms.push_back(term);
        }
    }

    if (isAndQuery && actualTerms.size() >= 2) {
        // AND operation - only documents containing ALL terms
        std::unordered_set<int> validDocs = andOperation(actualTerms);
        
        if (validDocs.empty()) {
            std::cout << "AND query returned no results (no documents contain all terms)\n";
            return {};
        }
        
        std::cout << "AND query found " << validDocs.size() << " documents with all terms\n";
        
        // Calculate scores only for documents that contain ALL terms
        for (const auto& term : actualTerms) {
            auto it = invertedIndex.find(term);
            if (it == invertedIndex.end()) continue;

            const auto& docFreqMap = it->second;
            int df = static_cast<int>(docFreqMap.size());

            for (const auto& [docId, freq] : docFreqMap) {
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
            if (it == invertedIndex.end()) {
                std::cout << "Term '" << term << "' not found in index\n";
                continue;
            }

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
    
    // Start with documents containing first term
    auto it = invertedIndex.find(terms[0]);
    if (it == invertedIndex.end()) {
        std::cout << "First AND term '" << terms[0] << "' not found\n";
        return {};
    }

    std::unordered_set<int> resultSet;
    for (const auto& [docId, freq] : it->second) {
        resultSet.insert(docId);
    }

    // Intersect with documents containing each subsequent term
    for (size_t i = 1; i < terms.size(); ++i) {
        auto it2 = invertedIndex.find(terms[i]);
        if (it2 == invertedIndex.end()) {
            std::cout << "AND term '" << terms[i] << "' not found - returning empty set\n";
            return {};
        }
        
        std::unordered_set<int> currentSet;
        for (const auto& [docId, freq] : it2->second) {
            currentSet.insert(docId);
        }

        // Keep only documents in both sets
        std::unordered_set<int> newSet;
        for (int docId : resultSet) {
            if (currentSet.count(docId)) {
                newSet.insert(docId);
            }
        }
        
        resultSet = std::move(newSet);
        
        if (resultSet.empty()) {
            std::cout << "Intersection became empty after term '" << terms[i] << "'\n";
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
        if (it == invertedIndex.end()) continue;
        
        for (const auto& [docId, freq] : it->second) {
            resultSet.insert(docId);
        }
    }
    
    return resultSet;
}