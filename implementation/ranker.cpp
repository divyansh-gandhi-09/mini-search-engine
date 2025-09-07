#include "ranker.h"
#include <cmath>

double Ranker::score(const std::string& /*term*/, int freq, int totalDocs, int df) {
    if (freq <= 0 || df == 0) return 0.0;

    // Term Frequency (TF)
    double tf = 1.0 + std::log(freq);

    // Inverse Document Frequency (IDF)
    double idf = std::log((double)(totalDocs + 1) / (df + 1)) + 1.0;

    return tf * idf;
}
