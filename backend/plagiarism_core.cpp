#include "plagiarism_core.h"

static const uint64_t MOD  = 1000000007ULL;
static const uint64_t BASE = 911382323ULL;

// ---------------- Normalize (strip comments, compress spaces) ----------------
string normalizeStrip(const string &s) {
    string out; out.reserve(s.size());
    bool in_sl = false, in_ml = false;

    for (size_t i = 0; i < s.size(); ++i) {
        if (!in_sl && !in_ml && i + 1 < s.size() && s[i] == '/' && s[i+1] == '/') { in_sl = true; ++i; continue; }
        if (!in_sl && !in_ml && i + 1 < s.size() && s[i] == '/' && s[i+1] == '*') { in_ml = true; ++i; continue; }
        if (in_sl && s[i] == '\n') { in_sl = false; out.push_back('\n'); continue; }
        if (in_ml && i + 1 < s.size() && s[i] == '*' && s[i+1] == '/') { in_ml = false; ++i; continue; }
        if (in_sl || in_ml) continue;

        unsigned char ch = static_cast<unsigned char>(s[i]);
        if (isspace(ch)) {
            if (!out.empty() && out.back() != ' ') out.push_back(' ');
        } else {
            out.push_back((char)ch);
        }
    }
    if (!out.empty() && out.front() == ' ') out.erase(out.begin());
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// ---------------- Tokenize (ident/num/underscore vs single-char ops) ---------
vector<string> tokenize(const string &code) {
    vector<string> tokens;
    string cur;
    for (unsigned char c : code) {
        if (isalnum(c) || c == '_') {
            cur.push_back((char)c);
        } else {
            if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
            if (!isspace(c)) tokens.push_back(string(1, (char)c));
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

// ---------------- Fingerprints via rolling hash + winnowing ------------------
vector<uint64_t> fingerprintTokens(const vector<string> &tokens, int window) {
    vector<uint64_t> hashes;
    if (tokens.empty() || window <= 0) return hashes;

    // Map tokens to integer ids (stable vocabulary in this run)
    unordered_map<string,int> id;
    id.reserve(tokens.size()*2);
    vector<int> ids; ids.reserve(tokens.size());
    int nxt = 1;
    for (auto &t : tokens) {
        auto it = id.find(t);
        if (it == id.end()) it = id.emplace(t, nxt++).first;
        ids.push_back(it->second);
    }

    // Rolling hash of k=1 token (simple and fast)
    vector<uint64_t> roll(ids.size());
    uint64_t h = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        h = (h * BASE + (uint64_t)ids[i]) % MOD;
        roll[i] = h;
    }

    if ((int)roll.size() < window) return {};

    // Winnowing: min hash per sliding window (classic monotonic queue)
    deque<pair<uint64_t,int>> dq;
    for (int i = 0; i < (int)roll.size(); ++i) {
        while (!dq.empty() && dq.back().first >= roll[i]) dq.pop_back();
        dq.emplace_back(roll[i], i);

        int L = i - window + 1;
        if (L >= 0) {
            while (!dq.empty() && dq.front().second < L) dq.pop_front();
            hashes.push_back(dq.front().first);
        }
    }
    return hashes;
}

// ---------------- Jaccard over fingerprint sets ------------------------------
double jaccardFingerprint(const vector<uint64_t> &a, const vector<uint64_t> &b) {
    if (a.empty() && b.empty()) return 1.0; // both empty -> identical
    unordered_set<uint64_t> A(a.begin(), a.end()), B(b.begin(), b.end());
    size_t inter = 0;
    for (auto &x : A) if (B.count(x)) ++inter;
    size_t uni = A.size() + B.size() - inter;
    if (uni == 0) return 0.0;
    return (double)inter / (double)uni;
}

// ---------------- High-level pair analysis -----------------------------------
AnalyzeResult analyzePair(const string& codeAraw, const string& codeBraw, int window) {
    AnalyzeResult R;

    string A = normalizeStrip(codeAraw);
    string B = normalizeStrip(codeBraw);

    auto tokA = tokenize(A);
    auto tokB = tokenize(B);

    R.tokensA = (int)tokA.size();
    R.tokensB = (int)tokB.size();

    auto fpA = fingerprintTokens(tokA, window);
    auto fpB = fingerprintTokens(tokB, window);

    R.fpsA = (int)fpA.size();
    R.fpsB = (int)fpB.size();
    R.jaccard = jaccardFingerprint(fpA, fpB);

    ostringstream oss;
    oss << "tokensA=" << R.tokensA << ", tokensB=" << R.tokensB
        << ", fpsA=" << R.fpsA << ", fpsB=" << R.fpsB;
    R.message = oss.str();
    return R;
}

// =============================================================================
// CLI MAIN — robust, non-interactive protocol (NO prompts, NO tildes)
// Protocol (stdin):
//   Line 1: "LENA <number>\n"
//   Line 2: "LENB <number>\n"
//   Then:   <number> raw bytes for A, followed by <number> raw bytes for B.
// Optional env WINDOW sets winnowing window (default 4).
// Output: single JSON line with fields: jaccard, tokensA, tokensB, fpsA, fpsB, message
// =============================================================================
static string readExact(istream& in, size_t n) {
    string s; s.resize(n);
    in.read(&s[0], (std::streamsize)n);
    if ((size_t)in.gcount() != n) s.resize((size_t)in.gcount());
    return s;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Parse headers
    string line1, line2;
    if (!getline(cin, line1) || !getline(cin, line2)) {
        cout << "{\"error\":\"Bad header. Expected LENA/LENB lines.\"}\n";
        return 0;
    }

    auto parseLen = [](const string& ln, const string& key)->long long{
        // expects "KEY <num>"
        size_t p = ln.find(' ');
        if (p == string::npos) return -1;
        string k = ln.substr(0, p);
        if (k != key) return -1;
        try { return stoll(ln.substr(p+1)); } catch (...) { return -1; }
    };

    long long la = parseLen(line1, "LENA");
    long long lb = parseLen(line2, "LENB");
    if (la < 0 || lb < 0) {
        cout << "{\"error\":\"Malformed lengths.\"}\n";
        return 0;
    }

    string A = readExact(cin, (size_t)la);
    string B = readExact(cin, (size_t)lb);
    if ((long long)A.size() != la || (long long)B.size() != lb) {
        cout << "{\"error\":\"Truncated input.\"}\n";
        return 0;
    }

    int window = 4;
    if (const char* wenv = getenv("WINDOW")) {
        try { window = max(1, stoi(wenv)); } catch (...) {}
    }

    auto R = analyzePair(A, B, window);

    auto safe = [](double x) { return std::isfinite(x) ? x : 0.0; };

    cout.setf(std::ios::fixed); cout << setprecision(6);
    cout << "{"
         << "\"jaccard\":" << safe(R.jaccard)
         << ",\"tokensA\":" << R.tokensA
         << ",\"tokensB\":" << R.tokensB
         << ",\"fpsA\":" << R.fpsA
         << ",\"fpsB\":" << R.fpsB
         << ",\"message\":\"" << R.message << "\""
         << "}\n";
    return 0;
}
