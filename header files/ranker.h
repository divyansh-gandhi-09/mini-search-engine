#pragma once
#include <string>
#include <vector>
#include <cmath>

class Ranker {
public:
    // Score a single term occurrence
    // freq = term frequency in a doc
    // totalDocs = total number of documents
    // df = number of docs containing this term
    static double score(const std::string& term, int freq, int totalDocs, int df);
};
