#ifndef PLAGIARISM_CORE_H
#define PLAGIARISM_CORE_H

#include <bits/stdc++.h>
using namespace std;

struct AnalyzeResult {
    double jaccard = 0.0;
    int tokensA = 0, tokensB = 0;
    int fpsA = 0, fpsB = 0;
    string message;
};

string normalizeStrip(const string& s);
vector<string> tokenize(const string& code);
vector<uint64_t> fingerprintTokens(const vector<string>& tokens, int window);
double jaccardFingerprint(const vector<uint64_t>& a, const vector<uint64_t>& b);
AnalyzeResult analyzePair(const string& codeA, const string& codeB, int window);

#endif
