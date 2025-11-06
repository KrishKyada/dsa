#include "plagiarism_core.h"

static const uint64_t MOD  = 1000000007ULL;
static const uint64_t BASE = 911382323ULL;

// ---------- Normalize code ----------
string normalizeStrip(const string& s){
    string out; out.reserve(s.size());
    bool in_sl=false, in_ml=false;
    for (size_t i=0;i<s.size();++i){
        if (!in_sl && !in_ml && i+1<s.size() && s[i]=='/' && s[i+1]=='/'){ in_sl=true; ++i; continue; }
        if (!in_sl && !in_ml && i+1<s.size() && s[i]=='/' && s[i+1]=='*'){ in_ml=true; ++i; continue; }
        if (in_sl && s[i]=='\n'){ in_sl=false; out.push_back('\n'); continue; }
        if (in_ml && i+1<s.size() && s[i]=='*' && s[i+1]=='/'){ in_ml=false; ++i; continue; }
        if (in_sl || in_ml) continue;
        if (isspace((unsigned char)s[i])){
            if (!out.empty() && out.back()!=' ') out.push_back(' ');
        } else out.push_back(s[i]);
    }
    if (!out.empty() && out.front()==' ') out.erase(out.begin());
    if (!out.empty() && out.back()==' ') out.pop_back();
    return out;
}

// ---------- Tokenization ----------
static const unordered_set<string> KEYWORDS = {
    "auto","break","case","char","const","continue","default","do","double","else",
    "enum","extern","float","for","goto","if","inline","int","long","register","restrict",
    "return","short","signed","sizeof","static","struct","switch","typedef","union",
    "unsigned","void","volatile","while","class","namespace","using","template","typename",
    "public","private","protected","virtual","override","final","include","define"
};

vector<string> tokenizeAndNormalize(const string& s){
    vector<string> tok;
    string cur;
    auto flush = [&](){
        if (!cur.empty()){
            if (isalpha((unsigned char)cur[0]) || cur[0]=='_'){
                if (!KEYWORDS.count(cur)) cur = "VAR";
            }
            tok.push_back(cur);
            cur.clear();
        }
    };
    for (char ch: s){
        if (isalnum((unsigned char)ch) || ch=='_'){ cur.push_back(ch); }
        else {
            flush();
            if (!isspace((unsigned char)ch)){
                string t(1, ch);
                tok.push_back(t);
            }
        }
    }
    flush();
    return tok;
}

// ---------- Rolling Hash + Winnowing ----------
vector<int> toIds(const vector<string>& tok, unordered_map<string,int>& vocab){
    vector<int> ids;
    ids.reserve(tok.size());
    for (auto &t: tok){
        if (!vocab.count(t)) vocab[t] = (int)vocab.size() + 1;
        ids.push_back(vocab[t]);
    }
    return ids;
}

static inline uint64_t mulmod(uint64_t a, uint64_t b){
#if defined(__SIZEOF_INT128__)
    __uint128_t res = ( __uint128_t)a * b;
    return (uint64_t)(res % MOD);
#else
    // Fallback (BASE, ids, MOD are small enough; this is safe here)
    return (a % MOD) * (b % MOD) % MOD;
#endif
}

vector<uint64_t> kgramHashes(const vector<int>& ids, int k){
    vector<uint64_t> res;
    if (k <= 0 || (int)ids.size() < k) return res;

    uint64_t power = 1;
    for (int i=0;i<k-1;i++) power = mulmod(power, BASE);

    uint64_t h = 0;
    for (int i=0;i<k;i++){
        h = (mulmod(h, BASE) + (uint64_t)(ids[i] % (int)MOD)) % MOD;
    }
    res.push_back(h);

    for (size_t i=k;i<ids.size();i++){
        uint64_t out = mulmod((uint64_t)(ids[i-k] % (int)MOD), power);
        h = ( (h + MOD) - out ) % MOD;
        h = (mulmod(h, BASE) + (uint64_t)(ids[i] % (int)MOD)) % MOD;
        res.push_back(h);
    }
    return res;
}

struct FP{ uint64_t hash; int pos; };

vector<FP> winnow(const vector<uint64_t>& h, int w){
    vector<FP> fps;
    if (h.empty() || w <= 0) return fps;

    deque<pair<uint64_t,int>> dq;
    for (int i=0;i<(int)h.size();i++){
        while (!dq.empty() && dq.back().first >= h[i]) dq.pop_back();
        dq.push_back({h[i], i});
        int L = i - w + 1;
        if (L >= 0){
            while (!dq.empty() && dq.front().second < L) dq.pop_front();
            fps.push_back({dq.front().first, dq.front().second});
        }
    }
    return fps;
}

double jaccard(const vector<FP>& A, const vector<FP>& B){
    unordered_set<uint64_t> SA, SB;
    SA.reserve(A.size()*2+1);
    SB.reserve(B.size()*2+1);
    for (auto &a: A) SA.insert(a.hash);
    for (auto &b: B) SB.insert(b.hash);

    if (SA.empty() && SB.empty()) return 1.0; // both empty => treat as identical

    size_t inter = 0;
    if (SA.size() < SB.size()){
        for (auto &x: SA) if (SB.count(x)) inter++;
    } else {
        for (auto &x: SB) if (SA.count(x)) inter++;
    }
    size_t uni = SA.size() + SB.size() - inter;
    if (uni == 0) return 1.0; // safety
    return (double)inter / (double)uni;
}

// ---------- Edit Distance ----------
int editDistance(const vector<int>& a, const vector<int>& b){
    int n = (int)a.size(), m = (int)b.size();
    if (n==0) return m;
    if (m==0) return n;

    vector<int> prev(m+1), cur(m+1);
    iota(prev.begin(), prev.end(), 0);
    for (int i=1;i<=n;i++){
        cur[0] = i;
        for (int j=1;j<=m;j++){
            if (a[i-1]==b[j-1]) cur[j] = prev[j-1];
            else cur[j] = 1 + min({prev[j], cur[j-1], prev[j-1]});
        }
        swap(prev, cur);
    }
    return prev[m];
}

// ---------- AST Comparison ----------
struct Node{
    string type; vector<Node*> child;
};

Node* buildAST(const vector<string>& tok){
    Node* root = new Node{"root", {}};
    stack<Node*> st; st.push(root);
    for (auto &t: tok){
        if (t=="if" || t=="for" || t=="while"){
            Node* n = new Node{t, {}};
            st.top()->child.push_back(n);
            st.push(n);
        } else if (t=="{"){
            Node* n = new Node{"block", {}};
            st.top()->child.push_back(n);
            st.push(n);
        } else if (t=="}"){
            if (st.size()>1) st.pop();
        } else if (t==";"){
            st.top()->child.push_back(new Node{"stmt", {}});
        }
    }
    while(st.size()>1) st.pop();
    return root;
}

double compareAST(Node* a, Node* b){
    if (!a || !b) return 0.0;
    if (a->type != b->type) return 0.0;

    int n = (int)a->child.size();
    int m = (int)b->child.size();
    if (n==0 && m==0) return 1.0;

    int upto = min(n, m);
    int match = 0;
    for (int i=0;i<upto;i++){
        if (compareAST(a->child[i], b->child[i]) > 0.9) match++;
    }
    int denom = max(n, m);
    if (denom == 0) return 1.0; // safety
    return 0.3 + 0.7 * (double)match / (double)denom;
}

void freeAST(Node* n){
    if (!n) return;
    for (auto c: n->child) freeAST(c);
    delete n;
}

// ---------- Main Analyzer ----------
AnalyzeResult analyzePair(const string& codeA, const string& codeB, int k, int w){
    AnalyzeResult R;

    string A = normalizeStrip(codeA);
    string B = normalizeStrip(codeB);

    auto Atok = tokenizeAndNormalize(A);
    auto Btok = tokenizeAndNormalize(B);

    R.tokensA = (int)Atok.size();
    R.tokensB = (int)Btok.size();

    unordered_map<string,int> vocab;
    auto Aids = toIds(Atok, vocab);
    auto Bids = toIds(Btok, vocab);

    // Hashes & Winnowing (guard k, w)
    if (k <= 0) k = 5;
    if (w <= 0) w = 4;

    auto Ah = kgramHashes(Aids, k);
    auto Bh = kgramHashes(Bids, k);

    auto Af = winnow(Ah, w);
    auto Bf = winnow(Bh, w);

    R.fpsA = (int)Af.size();
    R.fpsB = (int)Bf.size();

    R.jaccard = jaccard(Af, Bf);

    // Edit similarity; avoid 0/0
    int ed = editDistance(Aids, Bids);
    int maxLen = max((int)Aids.size(), (int)Bids.size());
    if (maxLen == 0) {
        R.editSim = 1.0; // both empty
    } else {
        R.editSim = max(0.0, 1.0 - (double)ed / (double)maxLen);
    }

    Node* astA = buildAST(Atok);
    Node* astB = buildAST(Btok);
    R.structSim = compareAST(astA, astB);
    freeAST(astA); freeAST(astB);

    // Final score
    R.score = 0.6 * (0.7 * R.jaccard + 0.3 * R.editSim) + 0.4 * R.structSim;

    ostringstream msg;
    msg << "Winnowing Jaccard=" << R.jaccard
        << ", EditSim=" << R.editSim
        << ", StructSim=" << R.structSim
        << " | tokensA=" << R.tokensA << ", tokensB=" << R.tokensB
        << ", fpsA=" << R.fpsA << ", fpsB=" << R.fpsB;
    R.message = msg.str();
    return R;
}

// ---------- Tiny JSON helpers for CLI ----------
static bool jsonGetString(const string& in, const string& key, string& out){
    // looks for "key":"value"
    string pat = "\"" + key + "\"";
    size_t p = in.find(pat);
    if (p == string::npos) return false;
    p = in.find(':', p);
    if (p == string::npos) return false;
    p = in.find('"', p);
    if (p == string::npos) return false;
    size_t e = in.find('"', p+1);
    if (e == string::npos) return false;
    out = in.substr(p+1, e-p-1);
    return true;
}

static bool jsonGetInt(const string& in, const string& key, int& out){
    // accepts "key":123  OR "key":"123"
    string pat = "\"" + key + "\"";
    size_t p = in.find(pat);
    if (p == string::npos) return false;
    p = in.find(':', p);
    if (p == string::npos) return false;

    // skip spaces
    size_t i = p+1;
    while (i < in.size() && isspace((unsigned char)in[i])) i++;

    if (i < in.size() && in[i] == '"'){
        size_t e = in.find('"', i+1);
        if (e == string::npos) return false;
        string num = in.substr(i+1, e-(i+1));
        try { out = stoi(num); return true; } catch (...) { return false; }
    } else {
        size_t e = i;
        while (e < in.size() && (isdigit((unsigned char)in[e]) || in[e]=='-' || in[e]=='+')) e++;
        if (e == i) return false;
        try { out = stoi(in.substr(i, e-i)); return true; } catch (...) { return false; }
    }
    return false;
}

// ---------- CLI Main for Flask ----------
#ifdef BUILD_MAIN
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string input, line;
    while (getline(cin, line)) {
        input += line;
        input.push_back('\n');
    }

    string codeA, codeB;
    int k = 5, w = 4;

    // robust(ish) extraction
    jsonGetString(input, "codeA", codeA);
    jsonGetString(input, "codeB", codeB);
    jsonGetInt(input, "k", k);
    jsonGetInt(input, "w", w);

    auto R = analyzePair(codeA, codeB, k, w);

    cout.setf(std::ios::fixed); cout << setprecision(6);
    cout << "{"
         << "\"score\":"     << R.score     << ","
         << "\"jaccard\":"   << R.jaccard   << ","
         << "\"editSim\":"   << R.editSim   << ","
         << "\"structSim\":" << R.structSim << ","
         << "\"tokensA\":"   << R.tokensA   << ","
         << "\"tokensB\":"   << R.tokensB   << ","
         << "\"fpsA\":"      << R.fpsA      << ","
         << "\"fpsB\":"      << R.fpsB      << ","
         << "\"message\":\"" << R.message   << "\""
         << "}";
    return 0;
}
#endif
