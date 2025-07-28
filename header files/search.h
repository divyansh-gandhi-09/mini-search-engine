#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ranker.h"
using namespace std;    
class SearchEngine {
public:
    SearchEngine(const unordered_map<string, unordered_map<int, int>>
    &  index, int totalDocs);
    vector<pair<int, double>> search(const string& query, const Ranker
    & ranker);
private:
    unordered_map<string, unordered_map<int, int>> invertedIndex;
    int totalDocuments;

    unordered_set<int> andOperation(const vector<string>& terms);
    unordered_set<int> orOperation(const vector<string>& terms);
};
