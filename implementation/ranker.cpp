#include "ranker.h"
using namespace std;
vector<pair<int, double>> Ranker::rank(
    const string& query,
    const unordered_map<string, unordered_map<int, int>>& index,
    int totalDocs
) {
    unordered_map<int, double> docScores;
    unordered_map<int, int> docLengths;
    istringstream iss(query);
    string term;
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
    vector<pair<int, double>> ranked(docScores.begin(), docScores.end());
    sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });
    return ranked;
}
