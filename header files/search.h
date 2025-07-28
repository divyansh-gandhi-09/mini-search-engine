#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ranker.h"
class SearchEngine {
public:
    SearchEngine(const std::unordered_map<std::string, std::unordered_map<int, int>>
    &  index, int totalDocs);
    std::vector<std::pair<int, double>> search(const std::string& query, const Ranker
    & ranker);
private:
    std::unordered_map<std::string, std::unordered_map<int, int>> invertedIndex;
    int totalDocuments;

    std::unordered_set<int> andOperation(const std::vector<std::string>& terms);
    std::unordered_set<int> orOperation(const std::vector<std::string>& terms);
};
