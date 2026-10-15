#define BELIEF_NO_MAIN
#include "belief_bot.cpp"
#include <iomanip>

// Deliberate information boundary. No actual opponent card or draw order survives.
// The own remaining multiset is known in the supported NR/NG rules (no reshuffle).
Game observation(const Game& state, int viewer) {
    Game g=state;
    int o=1-viewer;
    g.p[o].hand.assign(state.p[o].hand.size(),-1);
    g.p[o].deck.assign(state.p[o].deck.size(),-1);
    sort(g.p[viewer].deck.begin(),g.p[viewer].deck.end());
    g.rng=Rng(0);
    return g;
}
string deckSignature(const Deck& d) {
    auto c=d.cards; sort(c.begin(),c.end());
    string key=to_string(d.faction)+":"+to_string(d.leader);
    for(int x:c) key+=","+to_string(x);
    return key;
}
vector<Deck> libraryV6(const vector<Deck>& input) {
    auto lib=expandLibrary(input), seeds=lib;
    set<string> seen;
    for(auto& d:lib) seen.insert(deckSignature(d));
    auto append=[&](Deck d) {
        int specials=0; for(int c:d.cards) specials+=!!C[c].kind;
        if(specials<=10&&seen.insert(deckSignature(d)).second) lib.push_back(d);
    };
    // Include hidden additions and substitutions, notably Fog + Rain together.
    // Equal prior over distinct lists is a modelling assumption, not human frequency.
    for(auto d:seeds) for(int weather:{id("Impenetrable Fog"),id("Torrential Rain")}) {
        Deck added=d; added.name+="_plus_"+to_string(weather);
        added.cards.push_back(weather); append(added);
        set<int> replaced;
        for(int i=0;i<(int)d.cards.size();++i) {
            int c=d.cards[i];
            if(!C[c].kind||c==DEC||c==weather||!replaced.insert(c).second) continue;
            Deck changed=d; changed.name+="_swap_"+to_string(c)+"_"+to_string(weather);
            changed.cards[i]=weather; append(changed);
        }
    }
    return lib;
}
string actionKey(Act a) {
    return to_string(a.kind)+","+to_string(a.card)+","+to_string(a.target)+","+to_string(a.row);
}
string actionLabel(const Game& g,Act a) {
    if(a.kind==3) return "PASS";
    if(a.kind==1) return "LEADER"+(a.target>=0?string(" -> ")+C[a.target].name:"");
    string s=(a.kind==2?"REVIVE ":"")+C[a.card].name;
    if(a.target>=0) s+=" -> "+C[g.p[g.turn].board[a.target].id].name;
    if(a.row>=0) s+=" row="+to_string(a.row);
    return s;
}
void putCards(ostringstream& out,vector<int> cards) {
    sort(cards.begin(),cards.end()); for(int c:cards) out<<c<<','; out<<'/';
}
// Public history plus the acting player's observations. Neither enemy hand nor
// either ordered draw pile is a node key. Private observations have perfect recall.
string privateKey(const Game& g,int s) {
    ostringstream out;
    putCards(out,g.p[s].hand); putCards(out,g.p[s].deck);
    return out.str();
}
string publicKey(const Game& g) {
    ostringstream out;
    out<<g.turn<<','<<g.pending<<','<<g.round<<','<<g.winner<<','<<g.emhyrSpecial<<','<<g.heroThreshold<<';';
    for(auto& p:g.p) {
        out<<p.leader<<','<<p.wins<<','<<p.lhorn<<','<<p.used<<','<<p.passed<<','<<p.hand.size()<<','<<p.deck.size()<<':';
        putCards(out,p.grave); putCards(out,p.known);
        for(auto u:p.board) out<<u.id<<','<<u.row<<';'; out<<'|';
    }
    for(auto w:g.weather) out<<w.first<<','<<w.second<<';';
    return out.str();
}
bool publiclyKnownEndgame(const Game& g) {
    if(!solvable(g)) return false;
    for(int s=0;s<2;s++) {
        auto hand=g.p[s].hand,known=g.p[s].known;
        sort(hand.begin(),hand.end());sort(known.begin(),known.end());
        if(hand!=known) return false;
    }
    return true;
}
struct SearchConfig {int iterations=512,treeDepth=80,exactBudget=100000; double exploration=1.25;};
struct Branch {Act action; int visits=0; double wins=0;};
struct InfoNode {int visits=0; vector<Branch> branches;};
struct SearchReport {
    Act chosen; bool exact=false,guaranteed=false; int hypotheses=0,iterations=0,nodes=0;
    vector<Branch> root;
};

// Bounded AND/OR solver for hidden-list endgames, WITHOUT strategy fusion.
// A single own action must work across all still possible worlds. Observed enemy
// actions eliminate incompatible worlds; future choices may use that evidence.
// Opponent is worst-case (even if informed of our hand): a positive result is a
// guarantee within the enumerated library. A negative result is NOT a loss odds.
struct RobustSolver {
    int viewer,budget,visited=0;
    unordered_map<string,bool> memo;
    RobustSolver(int side,int cap):viewer(side),budget(cap) {}
    string key(const vector<Game>& worlds) {
        vector<string> keys;for(auto& g:worlds)keys.push_back(stateKey(g));
        sort(keys.begin(),keys.end());keys.erase(unique(keys.begin(),keys.end()),keys.end());
        string out;for(auto& k:keys)out+="["+k+"]";return out;
    }
    bool branchesWin(const vector<Game>& worlds,Act action,bool enemy) {
        map<string,vector<Game>> observations;
        for(auto g:worlds) {
            auto actions=legal(g);
            if(find(actions.begin(),actions.end(),action)==actions.end()) {
                if(!enemy) return false;
                continue;
            }
            apply(g,action);
            observations[publicKey(g)+"#"+privateKey(g,viewer)].push_back(g);
        }
        for(auto& group:observations) if(!wins(group.second))return false;
        return !observations.empty();
    }
    bool wins(const vector<Game>& worlds) {
        if(++visited>budget)throw Limit{};
        if(worlds.empty())throw runtime_error("empty belief");
        for(auto& g:worlds)if(!g.p[0].deck.empty()||!g.p[1].deck.empty())
            throw runtime_error("robust exact solver requires empty draw piles");
        if(worlds.front().winner>=0) {
            for(auto& g:worlds)if(g.winner!=viewer)return false;
            return true;
        }
        auto k=key(worlds);auto hit=memo.find(k);if(hit!=memo.end())return hit->second;
        bool enemy=worlds.front().turn!=viewer;
        vector<Act> actions=legal(worlds.front());
        if(enemy) for(auto& g:worlds)for(auto a:legal(g))
            if(find(actions.begin(),actions.end(),a)==actions.end())actions.push_back(a);
        for(auto a:actions) {
            bool win=branchesWin(worlds,a,enemy);
            if(!enemy&&win)return memo[k]=true;
            if(enemy&&!win)return memo[k]=false;
        }
        return memo[k]=enemy;
    }
    bool move(const vector<Game>& worlds,Act& selected) {
        if(worlds.front().turn!=viewer)throw runtime_error("wrong viewer");
        for(auto a:legal(worlds.front()))if(branchesWin(worlds,a,false)){selected=a;return true;}
        return false;
    }
};
bool stripInactiveDecks(Game& g) {
    if(g.p[0].deck.empty()&&g.p[1].deck.empty())return true;
    // Sufficient, deliberately conservative condition: last round, spent leaders,
    // no playable draw/retrieval mechanism on either side. Deck order and contents
    // then cannot affect ANY legal continuation. No fictitious perfect-info draw.
    if(g.pending>=0||g.p[0].wins!=1||g.p[1].wins!=1||!g.p[0].used||!g.p[1].used)return false;
    for(auto& p:g.p)for(int c:p.hand)if(c==DEC||(C[c].flags&(SPY|MED|MUSTER)))return false;
    for(auto& p:g.p)p.deck.clear();return true;
}
bool robustMove(const Game& observed,const vector<Hypothesis>& hypotheses,Act& action,int budget) {
    if(observed.p[0].hand.size()+observed.p[1].hand.size()>8)return false;
    vector<Game> worlds;set<string> seen;int enumerated=0;
    int o=1-observed.turn;
    int need=observed.p[o].hand.size()-observed.p[o].known.size();
    bool supported=true;
    for(auto& h:hypotheses) {
        auto pool=h.unknown;sort(pool.begin(),pool.end());vector<int> chosen;
        // Enumerate ALL distinct possible hand multisets, never a random subset
        // for a guarantee. Hard caps cause an explicit unproved fallback.
        auto visit=[&](auto&& self,int start,int left)->void {
            if(!supported)return;
            if(left==0) {
                if(++enumerated>4096){supported=false;return;}
                Game g=observed;g.p[o].hand=g.p[o].known;
                g.p[o].hand.insert(g.p[o].hand.end(),chosen.begin(),chosen.end());
                g.p[o].deck=pool;for(int c:chosen)erase1(g.p[o].deck,c);
                if(!stripInactiveDecks(g)){supported=false;return;}
                if(seen.insert(stateKey(g)).second)worlds.push_back(g);
                if(worlds.size()>32)supported=false;
                return;
            }
            for(int i=start;i+left<=(int)pool.size()&&supported;i++) {
                if(i>start&&pool[i]==pool[i-1])continue;
                chosen.push_back(pool[i]);self(self,i+1,left-1);chosen.pop_back();
            }
        };
        visit(visit,0,need);if(!supported)return false;
    }
    if(worlds.empty())return false;
    try {RobustSolver solver(observed.turn,budget);return solver.move(worlds,action);}
    catch(Limit&){return false;}
}

// Multi-observer information-set search, with one fresh compatible world per
// iteration. Both players maximize their own outcomes. This is NOT a Nash solver:
// root conditioning, finite particles and heuristic rollouts remain limitations.
SearchReport decideV6(const Game& actual,const Deck& own,const vector<Deck>& library,
                      uint64_t seed,SearchConfig cfg={}) {
    if(cfg.iterations<1||cfg.treeDepth<1) throw runtime_error("invalid search budget");
    const int viewer=actual.turn;
    Game observed=observation(actual,viewer);
    auto hypotheses=compatible(observed,viewer,own,library);
    if(hypotheses.empty()) throw runtime_error("unsupported public evidence: expand deck library");
    Rng rng(seed);
    Game initial=sampleBelief(observed,viewer,hypotheses,rng);
    SearchReport report;report.hypotheses=hypotheses.size();
    bool same=true;for(auto& h:hypotheses) same&=h.unknown==hypotheses.front().unknown;
    // Empty piles alone do not reveal a secretly modified list. Require agreement
    // of EVERY compatible hypothesis before claiming a deterministic exact result.
    if(same&&exactMove(initial,report.chosen,cfg.exactBudget)) {
        report.exact=true; return report;
    }
    if(robustMove(observed,hypotheses,report.chosen,cfg.exactBudget)) {
        report.guaranteed=true;return report;
    }
    auto moves=legal(initial);
    if(moves.size()==1) {report.chosen=moves.front();return report;}
    unordered_map<string,InfoNode> tree;
    const string rootKey="ROOT";
    for(int iteration=0;iteration<cfg.iterations;iteration++) {
        Game g=sampleBelief(observed,viewer,hypotheses,rng);
        string history;
        array<string,2> memories={privateKey(g,0),privateKey(g,1)};
        vector<pair<string,int>> path;
        for(int depth=0;g.winner<0&&depth<cfg.treeDepth;depth++) {
            int actor=g.turn;
            string key=depth==0?rootKey:history+"#"+publicKey(g)+"#"+memories[actor];
            auto& node=tree[key];
            auto actions=legal(g);
            if(node.branches.empty()) {
                // All legal choices, including every medic target, are retained.
                for(auto a:actions) node.branches.push_back({a});
            }
            assert(node.branches.size()==actions.size());
            int selected=-1;double best=-1e100;
            for(int j=0;j<(int)node.branches.size();j++) {
                auto& e=node.branches[j];
                double value=e.visits ? (actor==viewer?e.wins/e.visits:1-e.wins/e.visits)
                    +cfg.exploration*sqrt(log(node.visits+1.0)/e.visits) : 1e9+rng.u();
                if(value>best) {best=value;selected=j;}
            }
            auto action=node.branches[selected].action;
            bool fresh=node.branches[selected].visits==0;
            path.push_back({key,selected});
            apply(g,action);
            history+=";"+to_string(actor)+":"+actionKey(action);
            for(int s=0;s<2;s++) memories[s]+="|"+privateKey(g,s);
            if(fresh) break;
        }
        int winner=g.winner;
        if(winner<0&&publiclyKnownEndgame(g)) {
            try {Solver solver(1000);winner=solver.value(g)==1?0:1;} catch(Limit&) {}
        }
        if(winner<0) {
            // Vary OUR and THEIR rollout styles independently; never treat the
            // sampled hidden hand as public knowledge for a perfect-info oracle.
            int ours=iteration%3,theirs=(iteration/3)%3;
            while(g.winner<0) {
                if(g.steps>500) throw runtime_error("rollout did not terminate");
                apply(g,policy(g,rng,g.turn==viewer?ours:theirs));
            }
            winner=g.winner;
        }
        for(auto& step:path) {
            auto& node=tree.at(step.first);auto& edge=node.branches[step.second];
            node.visits++;edge.visits++;edge.wins+=winner==viewer;
        }
    }
    report.root=tree.at(rootKey).branches;
    int best=0;
    for(int j=1;j<(int)report.root.size();j++)
        if(report.root[j].visits>report.root[best].visits) best=j;
    report.chosen=report.root[best].action;
    report.iterations=cfg.iterations; report.nodes=tree.size();
    return report;
}

struct SafeReport {
    SearchReport search;Act baseline,chosen;
    bool accepted=false;double gap=0,radius=0;int holdout=0;
};
// Candidate selection and confirmation use independent random streams. Only one
// fixed candidate is tested against baseline, at a prespecified holdout size.
// Hoeffding applies to paired rollout differences in [-1,1], NOT real-player odds.
SafeReport playV6(const Game& actual,const Deck& own,const vector<Deck>& library,
                  uint64_t seed,SearchConfig cfg={},int holdout=128) {
    if(holdout<1)throw runtime_error("invalid validation budget");
    SafeReport out;out.search=decideV6(actual,own,library,seed,cfg);
    Game obs=observation(actual,actual.turn);
    auto h=compatible(obs,obs.turn,own,library);
    Rng rng(seed^0xe6bb21e445a19c03ULL);
    Game first=sampleBelief(obs,obs.turn,h,rng);
    out.baseline=policy(first,rng,0);out.chosen=out.baseline;
    if(out.search.exact||out.search.guaranteed) {
        out.chosen=out.search.chosen;out.accepted=true;return out;
    }
    if(out.search.chosen==out.baseline)return out;
    int difference=0;
    for(int i=0;i<holdout;i++) {
        Game sampled=sampleBelief(obs,obs.turn,h,rng);auto rolloutSeed=rng.next();
        // Random independent styles make each paired draw identically distributed.
        int ours=rng.n(3),theirs=rng.n(3);
        int outcomes[2];Act choices[2]={out.baseline,out.search.chosen};
        for(int j=0;j<2;j++) {
            Game g=sampled;apply(g,choices[j]);Rng r(rolloutSeed);
            while(g.winner<0) {
                if(g.steps>500)throw runtime_error("validation did not terminate");
                apply(g,policy(g,r,g.turn==obs.turn?ours:theirs));
            }
            outcomes[j]=g.winner==obs.turn;
        }
        difference+=outcomes[1]-outcomes[0];
    }
    out.holdout=holdout;out.gap=double(difference)/holdout;
    out.radius=sqrt(2*log(1/.05)/holdout);
    if(out.gap>out.radius){out.chosen=out.search.chosen;out.accepted=true;}
    return out;
}

#ifndef V6_NO_MAIN
int main(int argc,char**argv) {
    setup();tests();
    auto lib=libraryV6(loadDecks(argc>1?argv[1]:"decks.txt"));
    auto findDeck=[&](string name){return *find_if(lib.begin(),lib.end(),[&](const Deck& d){return d.name==name;});};
    Deck north=findDeck("N_siege_F0S2C1"),nilf=findDeck("G_impera_C1");
    Game g=start(north,nilf,65137);Rng rr(77);
    while(!(g.turn==0&&g.pending<0&&g.round==0&&g.steps>=8&&legal(g).size()>3)) apply(g,policy(g,rr,0));
    SearchConfig cfg;cfg.iterations=argc>2?stoi(argv[2]):512;
    auto begin=chrono::steady_clock::now();auto safe=playV6(g,north,lib,43321,cfg);auto result=safe.search;
    cout<<"library="<<lib.size()<<" hypotheses="<<result.hypotheses<<" iterations="<<result.iterations<<" nodes="<<result.nodes<<"\n";
    cout<<"chosen="<<actionLabel(g,result.chosen)<<" exact="<<result.exact<<"\n";
    for(auto e:result.root) cout<<actionLabel(g,e.action)<<" visits="<<e.visits<<" rollout_wins="<<e.wins<<"\n";
    cout<<"protected="<<actionLabel(g,safe.chosen)<<" gap="<<safe.gap<<" radius="<<safe.radius<<" accepted="<<safe.accepted<<"\n";
    cout<<"seconds="<<chrono::duration<double>(chrono::steady_clock::now()-begin).count()<<"\n";
}
#endif
