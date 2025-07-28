#include "ranker.h"
std::vector<std::pair<int, double>> Ranker::rank(
    const std::string& query,
    const std::unordered_map<std::string, std::unordered_map<int, int>>& index,
    int totalDocs
) {
    std::unordered_map<int, double> docScores;
    std::unordered_map<int, int> docLengths;
    std::istringstream iss(query);
    std::string term;
    while (iss >> term) {
        if (index.count(term)) {
            const auto& postingList = index.at(term);
            int df = postingList.size();
            double idf = log((double)(totalDocs + 1) / (df + 1)) + 1;
            for (const auto& [docID, tf] : postingList) {
                double tf_weight = 1 + log(tf);
                double score = tf_weight * idf;
                docScores[docID] += score;
            }
        }
    }
    std::vector<std::pair<int, double>> ranked(docScores.begin(), docScores.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    return ranked;
}
