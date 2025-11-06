#include "plagiarism_core.h"

static const uint64_t MOD = 1000000007ULL;
static const uint64_t BASE = 911382323ULL;

// ---------- Normalize code ----------
string normalizeStrip(const string& s){
    string out; out.reserve(s.size());
    bool in_sl=false, in_ml=false;
    for(size_t i=0;i<s.size();++i){
        if(!in_sl && !in_ml && i+1<s.size() && s[i]=='/' && s[i+1]=='/'){ in_sl=true; ++i; continue; }
        if(!in_sl && !in_ml && i+1<s.size() && s[i]=='/' && s[i+1]=='*'){ in_ml=true; ++i; continue; }
        if(in_sl && s[i]=='\n'){ in_sl=false; out.push_back('\n'); continue; }
        if(in_ml && i+1<s.size() && s[i]=='*' && s[i+1]=='/'){ in_ml=false; ++i; continue; }
        if(in_sl || in_ml) continue;
        if(isspace((unsigned char)s[i])){
            if(!out.empty() && out.back()!=' ') out.push_back(' ');
        } else out.push_back(s[i]);
    }
    if(!out.empty() && out.front()==' ') out.erase(out.begin());
    if(!out.empty() && out.back()==' ') out.pop_back();
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
        if(!cur.empty()){
            if(isalpha((unsigned char)cur[0]) || cur[0]=='_'){
                if(!KEYWORDS.count(cur)) cur = "VAR";
            }
            tok.push_back(cur);
            cur.clear();
        }
    };
    for(char ch: s){
        if(isalnum((unsigned char)ch) || ch=='_'){ cur.push_back(ch); }
        else {
            flush();
            if(!isspace((unsigned char)ch)){
                string t(1,ch);
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
    for(auto &t: tok){
        if(!vocab.count(t)) vocab[t]=(int)vocab.size()+1;
        ids.push_back(vocab[t]);
    }
    return ids;
}

vector<uint64_t> kgramHashes(const vector<int>& ids, int k){
    vector<uint64_t> res;
    if(ids.size()<k) return res;
    uint64_t power=1;
    for(int i=0;i<k-1;i++) power=(power*BASE)%MOD;
    uint64_t h=0;
    for(int i=0;i<k;i++) h=(h*BASE+ids[i])%MOD;
    res.push_back(h);
    for(size_t i=k;i<ids.size();i++){
        h=((h+MOD)-ids[i-k]*power%MOD)%MOD;
        h=(h*BASE+ids[i])%MOD;
        res.push_back(h);
    }
    return res;
}

struct FP{uint64_t hash;int pos;};
vector<FP> winnow(const vector<uint64_t>& h,int w){
    vector<FP> fps;
    if(h.empty()) return fps;
    deque<pair<uint64_t,int>> dq;
    for(int i=0;i<h.size();i++){
        while(!dq.empty() && dq.back().first>=h[i]) dq.pop_back();
        dq.push_back({h[i],i});
        int L=i-w+1;
        if(L>=0){
            while(!dq.empty() && dq.front().second<L) dq.pop_front();
            fps.push_back({dq.front().first,dq.front().second});
        }
    }
    return fps;
}

double jaccard(const vector<FP>& A,const vector<FP>& B){
    unordered_set<uint64_t> SA,SB;
    for(auto&a:A) SA.insert(a.hash);
    for(auto&b:B) SB.insert(b.hash);
    size_t inter=0;
    for(auto&x:SA) if(SB.count(x)) inter++;
    size_t uni=SA.size()+SB.size()-inter;
    if(uni==0) return 1.0;
    return (double)inter/uni;
}

// ---------- Edit Distance ----------
int editDistance(const vector<int>& a,const vector<int>& b){
    int n=a.size(),m=b.size();
    vector<int> prev(m+1),cur(m+1);
    iota(prev.begin(),prev.end(),0);
    for(int i=1;i<=n;i++){
        cur[0]=i;
        for(int j=1;j<=m;j++){
            if(a[i-1]==b[j-1]) cur[j]=prev[j-1];
            else cur[j]=1+min({prev[j],cur[j-1],prev[j-1]});
        }
        swap(prev,cur);
    }
    return prev[m];
}

// ---------- AST Comparison ----------
struct Node{
    string type; vector<Node*> child;
};
Node* buildAST(const vector<string>& tok){
    Node* root=new Node{"root",{}};
    stack<Node*> st; st.push(root);
    for(auto&t:tok){
        if(t=="if"||t=="for"||t=="while"){Node*n=new Node{t,{}};st.top()->child.push_back(n);st.push(n);}
        else if(t=="{"){Node*n=new Node{"block",{}};st.top()->child.push_back(n);st.push(n);}
        else if(t=="}"){if(st.size()>1) st.pop();}
        else if(t==";"){st.top()->child.push_back(new Node{"stmt",{}});}
    }
    while(st.size()>1) st.pop();
    return root;
}
double compareAST(Node*a,Node*b){
    if(!a||!b) return 0.0;
    if(a->type!=b->type) return 0.0;
    int n=a->child.size(),m=b->child.size();
    if(!n&&!m) return 1.0;
    int match=0;
    for(int i=0;i<min(n,m);i++)
        if(compareAST(a->child[i],b->child[i])>0.9) match++;
    return 0.3+0.7*(double)match/max(n,m);
}
void freeAST(Node*n){if(!n)return;for(auto c:n->child)freeAST(c);delete n;}

// ---------- Main Analyzer ----------
AnalyzeResult analyzePair(const string& codeA,const string& codeB,int k,int w){
    AnalyzeResult R;
    string A=normalizeStrip(codeA),B=normalizeStrip(codeB);
    auto Atok=tokenizeAndNormalize(A),Btok=tokenizeAndNormalize(B);
    unordered_map<string,int> vocab;
    auto Aids=toIds(Atok,vocab),Bids=toIds(Btok,vocab);
    auto Ah=kgramHashes(Aids,k),Bh=kgramHashes(Bids,k);
    auto Af=winnow(Ah,w),Bf=winnow(Bh,w);

    R.jaccard=jaccard(Af,Bf);
    int ed=editDistance(Aids,Bids);
    R.editSim=1.0-(double)ed/max(Aids.size(),Bids.size());
    Node* astA=buildAST(Atok); Node* astB=buildAST(Btok);
    R.structSim=compareAST(astA,astB); freeAST(astA); freeAST(astB);

    R.score=0.6*(0.7*R.jaccard+0.3*R.editSim)+0.4*R.structSim;
    ostringstream msg;
    msg<<"Winnowing Jaccard="<<R.jaccard<<", EditSim="<<R.editSim<<", StructSim="<<R.structSim;
    R.message=msg.str();
    return R;
}

// ---------- CLI Main for Flask ----------
#ifdef BUILD_MAIN
int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    string input; getline(cin,input);
    auto findVal=[&](const string& key){
        auto p=input.find("\""+key+"\"");
        if(p==string::npos) return string();
        p=input.find(':',p);
        p=input.find('"',p);
        auto e=input.find('"',p+1);
        return input.substr(p+1,e-p-1);
    };
    string codeA=findVal("codeA"),codeB=findVal("codeB");
    int k=5,w=4;
    auto R=analyzePair(codeA,codeB,k,w);
    cout<<"{\"score\":"<<R.score<<",\"jaccard\":"<<R.jaccard
        <<",\"editSim\":"<<R.editSim<<",\"structSim\":"<<R.structSim
        <<",\"message\":\""<<R.message<<"\"}";
    return 0;
}
#endif
