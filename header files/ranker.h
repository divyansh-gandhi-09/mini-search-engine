#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <algorithm>
using namespace std;
class Ranker {
public:
    static vector<pair<int, double>> rank(const string& query, 
    const unordered_map<string, unordered_map<int, int>>& index,
    int totalDocs);
};
