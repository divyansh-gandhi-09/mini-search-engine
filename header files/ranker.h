#pragma once
#include <unordered_map>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <algorithm>
class Ranker {
public:
    static std::vector<std::pair<int, double>> rank(const std::string& query,
    const std::unordered_map<std::string, std::unordered_map<int, int>>& index,
    int totalDocs);
};
