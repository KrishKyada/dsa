#ifndef PLAGIARISM_CORE_H
#define PLAGIARISM_CORE_H

#include <bits/stdc++.h>
using namespace std;

struct AnalyzeResult {
    double jaccard = 0.0, editSim = 0.0, structSim = 0.0, score = 0.0;
    int tokensA = 0, tokensB = 0, fpsA = 0, fpsB = 0;
    string message;
};

AnalyzeResult analyzePair(const string& codeA, const string& codeB, int k, int w);

#endif
