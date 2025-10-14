#include "search.h"
#include "parser.h"
#include <unordered_set>
#include <algorithm>
#include <iostream>

SearchEngine::SearchEngine(
    const std::unordered_map<std::string, std::unordered_map<int,int>>& index,
    int totalDocs)
    : invertedIndex(index), totalDocuments(totalDocs) {}

std::vector<std::pair<int,double>> SearchEngine::search(const std::string& query, const Ranker& ranker) {
    // Parser::tokenize already converts to lowercase
    std::vector<std::string> terms = Parser::tokenize(query);
    
    bool isAndQuery = false;
    std::vector<std::string> actualTerms;
    actualTerms.reserve(terms.size());
    
    // Separate "and" keyword from actual search terms
    for (const auto& term : terms) {
        if (term == "and") {
            isAndQuery = true;
        } else {
            actualTerms.push_back(term);
        }
    }

    std::unordered_map<int, double> scores;
    
    if (isAndQuery && actualTerms.size() >= 2) {
        // ✅ OPTIMIZED: Efficient AND operation with early termination
        std::unordered_set<int> validDocs = andOperation(actualTerms);
        
        if (validDocs.empty()) {
            std::cout << "AND query returned no results (no documents contain all terms)\n";
            return {};
        }
        
        std::cout << "AND query found " << validDocs.size() << " documents with all terms\n";
        
        // Pre-allocate scores map
        scores.reserve(validDocs.size());
        
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
        // ✅ OPTIMIZED: Pre-allocate based on estimated result size
        size_t estimatedResults = 0;
        for (const auto& term : actualTerms) {
            auto it = invertedIndex.find(term);
            if (it != invertedIndex.end()) {
                estimatedResults += it->second.size();
            }
        }
        scores.reserve(std::min<size_t>(estimatedResults, 10000));
        
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

    // ✅ OPTIMIZED: Reserve result vector
    std::vector<std::pair<int,double>> results;
    results.reserve(scores.size());
    results.assign(scores.begin(), scores.end());
    
    // ✅ Partial sort for top-K results (if you only need top 100)
    if (results.size() > 100) {
        std::partial_sort(results.begin(), results.begin() + 100, results.end(),
                          [](const auto& a, const auto& b){ return a.second > b.second; });
        results.resize(100);
    } else {
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b){ return a.second > b.second; });
    }
    
    return results;
}

// ✅ OPTIMIZED: Efficient AND operation with smallest-set-first strategy
std::unordered_set<int> SearchEngine::andOperation(const std::vector<std::string>& terms) {
    if (terms.empty()) return {};
    
    // ✅ Find the term with smallest posting list (optimization)
    size_t smallestIdx = 0;
    size_t smallestSize = SIZE_MAX;
    
    for (size_t i = 0; i < terms.size(); ++i) {
        auto it = invertedIndex.find(terms[i]);
        if (it == invertedIndex.end()) {
            std::cout << "AND term '" << terms[i] << "' not found - returning empty set\n";
            return {}; // Term not found = no results
        }
        if (it->second.size() < smallestSize) {
            smallestSize = it->second.size();
            smallestIdx = i;
        }
    }
    
    // Start with smallest set for efficiency
    auto startIt = invertedIndex.find(terms[smallestIdx]);
    std::unordered_set<int> resultSet;
    resultSet.reserve(startIt->second.size());
    
    for (const auto& [docId, _] : startIt->second) {
        resultSet.insert(docId);
    }
    
    // ✅ OPTIMIZED: Check containment efficiently
    for (size_t i = 0; i < terms.size(); ++i) {
        if (i == smallestIdx) continue; // Skip the one we started with
        
        auto it = invertedIndex.find(terms[i]);
        if (it == invertedIndex.end()) {
            return {}; // Early exit - no results possible
        }
        
        // Build hash set for O(1) lookup
        std::unordered_set<int> currentSet;
        currentSet.reserve(it->second.size());
        for (const auto& [docId, _] : it->second) {
            currentSet.insert(docId);
        }
        
        // Keep only documents in intersection
        std::unordered_set<int> intersection;
        intersection.reserve(std::min(resultSet.size(), currentSet.size()));
        
        for (int docId : resultSet) {
            if (currentSet.count(docId)) {
                intersection.insert(docId);
            }
        }
        
        resultSet = std::move(intersection);
        
        if (resultSet.empty()) {
            std::cout << "Intersection became empty after term '" << terms[i] << "'\n";
            return {}; // Early exit if empty
        }
    }
    
    return resultSet;
}

// ✅ OPTIMIZED: Union operation with reserves
std::unordered_set<int> SearchEngine::orOperation(const std::vector<std::string>& terms) {
    std::unordered_set<int> resultSet;
    
    // Estimate size for reserve
    size_t estimatedSize = 0;
    for (const auto& term : terms) {
        auto it = invertedIndex.find(term);
        if (it != invertedIndex.end()) {
            estimatedSize += it->second.size();
        }
    }
    resultSet.reserve(std::min<size_t>(estimatedSize, 100000));
    
    for (const std::string& term : terms) {
        auto it = invertedIndex.find(term);
        if (it == invertedIndex.end()) continue;
        
        for (const auto& [docId, _] : it->second) {
            resultSet.insert(docId);
        }
    }
    
    return resultSet;
}