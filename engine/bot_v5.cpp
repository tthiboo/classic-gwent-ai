#define main legacy_main
#include "sim_v3_source.cpp"
#undef main
#include <unordered_map>
#include <memory>
#include <chrono>
// Exact search is used only when both draw piles are empty. With public decklists,
// the remaining multiset of opponent cards is then inferable from public zones.
struct Limit{};
string stateKey(const Game&g){ostringstream s;s<<g.pendingKind<<','<<g.remaining<<','<<g.opening<<','<<g.turn<<','<<g.pending<<','<<g.round<<','<<g.winner<<','<<g.emhyrSpecial<<','<<g.heroThreshold<<';';for(auto&p:g.p){s<<p.faction<<','<<p.leader<<','<<p.wins<<','<<p.lhorn<<','<<p.used<<','<<p.passed<<':';for(auto v:{p.hand,p.grave}){sort(v.begin(),v.end());for(int x:v)s<<x<<',';s<<'/';}for(auto u:p.board)s<<u.id<<','<<u.row<<';';s<<'|';}for(auto w:g.weather)s<<w.first<<','<<w.second<<';';return s.str();}
struct Solver{int budget,visited=0;unordered_map<string,int> memo;Solver(int b):budget(b){}int value(const Game&g){if(g.winner>=0)return g.winner==2?0:g.winner==0?1:-1;if(++visited>budget)throw Limit{};auto key=stateKey(g);auto it=memo.find(key);if(it!=memo.end())return it->second;int best=g.turn==0?-1:1;for(auto a:legal(g)){Game z=g;apply(z,a);int v=value(z);best=g.turn==0?max(best,v):min(best,v);if(best==(g.turn==0?1:-1))break;}memo[key]=best;return best;}};
bool chanceRules(const Game&g){for(auto p:g.p)if(p.faction==2||p.faction==4||p.leader==9)return true;return g.pendingKind||g.opening;}
bool solvable(const Game&g){return !chanceRules(g)&&g.p[0].deck.empty()&&g.p[1].deck.empty()&&g.p[0].hand.size()+g.p[1].hand.size()<=8;}
bool exactMove(const Game&g,Act&chosen,int budget=16000){
 if(!solvable(g))return false;Solver sol(budget);int target=g.turn==0?1:-1,best=-2;auto actions=legal(g);chosen=actions.front();
 try{for(auto a:actions){Game z=g;apply(z,a);int value=target*sol.value(z);if(value>best){best=value;chosen=a;}if(best==1)return true;}return true;}
 catch(Limit&){return false;}
}
struct Edge{Act a;int n=0;double wins=0;unique_ptr<struct Node> child;Edge(Act x):a(x){}Edge(Edge&&)=default;};
struct Node{int n=0;vector<Edge> edges;};
// Open-loop information-set tree: edges contain actions, never the sampled hand.
// Each iteration draws a fresh hidden state. Opponent nodes maximize their own win rate.
Act treePolicy(const Game&g,Rng&r,int style,int iterations=160){Act solved;if(exactMove(g,solved))return solved;auto acts=legal(g);if(acts.size()==1)return acts[0];if(g.p[1-g.turn].passed&&ahead(g,g.turn)&&g.p[g.turn].wins==1&&g.pending<0)return {3};Node root;for(int k=0;k<iterations;k++){Game z=g;determinize(z,g.turn,r);Node* node=&root;vector<pair<Node*,int>> path;int depth=0;while(z.winner<0&&depth++<10){auto la=legal(z);vector<int> eligible;for(auto a:la){int idx=-1;for(int j=0;j<(int)node->edges.size();j++)if(node->edges[j].a==a){idx=j;break;}if(idx<0){idx=node->edges.size();node->edges.emplace_back(a);}eligible.push_back(idx);}int best=-1;double bs=-1e99;for(int j:eligible){auto&e=node->edges[j];double v=e.n?(z.turn==0?e.wins/e.n:1-e.wins/e.n)+1.1*sqrt(log(node->n+1.0)/e.n):1e6+r.u();if(v>bs){bs=v;best=j;}}auto&e=node->edges[best];bool fresh=e.n==0;path.push_back({node,best});apply(z,e.a);if(!e.child)e.child=make_unique<Node>();node=e.child.get();if(fresh)break;}
 int winner=z.winner;if(winner<0){bool done=false;if(solvable(z)){try{Solver sol(800);int value=sol.value(z);winner=value==0?2:value==1?0:1;done=true;}catch(Limit&){} }if(!done)winner=rollout(z,r,(style+k)%3);}for(auto [n,j]:path){n->n++;n->edges[j].n++;n->edges[j].wins+=winner==0;}}
 // Robust child: visits are more stable than a high mean from a single lucky rollout.
 int b=0;for(int i=1;i<(int)root.edges.size();i++)if(root.edges[i].n>root.edges[b].n)b=i;return root.edges[b].a;}
// Production candidate: broad root search, exact endgames; tree experiment retained separately.
int improvedRollout(Game g,Rng&r,int style){while(g.winner<0){if(g.steps>500)throw runtime_error("long rollout");if(solvable(g)&&g.p[0].hand.size()+g.p[1].hand.size()<=4){try{Solver sol(300);int value=sol.value(g);return value==0?2:value==1?0:1;}catch(Limit&){} }apply(g,policy(g,r,style));}return g.winner;}
Act strongPolicy(const Game&g,Rng&r,int style,int iterations=160){Act solved;if(exactMove(g,solved))return solved;auto a=legal(g);if(a.size()==1)return a[0];if(g.pending<0&&g.p[1-g.turn].passed&&ahead(g,g.turn)&&g.p[g.turn].wins==1)return {3};
 // Heuristic is a tie-break only. Every legal root action, including every medic target, is sampled.
 if(g.pending<0){auto ranked=rankActions(g,r,style);a.clear();for(auto q:ranked)a.push_back(q.a);}
 int trials=max(16,iterations/10);vector<double>w(a.size(),0);for(int k=0;k<trials;k++){Game d=g;determinize(d,g.turn,r);auto seed=r.next();for(int j=0;j<(int)a.size();j++){Game z=d;apply(z,a[j]);Rng rr(seed);w[j]+=improvedRollout(z,rr,(style+k)%3)==g.turn;}}
 int b=0;for(int j=1;j<(int)a.size();j++)if(w[j]>w[b])b=j;return a[b];}
Game basePuzzle(){Game g;g.p[0].wins=g.p[1].wins=1;g.p[0].used=g.p[1].used=true;return g;}
void audit(string deckpath){int total=0,oldok=0,newok=0,discriminating=0;Rng gen(8293);vector<int> pool={id("Catapult"),id("Crinfrid Reavers Dragon Hunter"),id("Blue Stripes Commando"),id("Geralt of Rivia"),id("Biting Frost"),id("Torrential Rain"),CLEAR,SCORCH,DEC,HORN,DRAG,id("Dun Banner Medic"),id("Thaler"),id("Impera Brigade Guard")};for(int i=0;i<180;i++){Game g=basePuzzle();g.turn=gen.n(2);for(int s=0;s<2;s++){g.p[s].lhorn=gen.n(2)?4:0;for(int j=0;j<2;j++){int c=pool[gen.n(pool.size())];g.p[s].hand.push_back(c);}for(int j=0;j<3;j++){int c=pool[gen.n(pool.size())];if(!C[c].kind)g.p[s].board.push_back({c,C[c].row==3?0:C[c].row});}for(int j=0;j<2;j++){int c=pool[gen.n(pool.size())];if(revivable(c))g.p[s].grave.push_back(c);}}if(i%4==0){g.pending=g.turn;g.p[g.turn].grave={id("Catapult"),id("Thaler"),id("Dun Banner Medic")};}Solver oracle(150000);try{int target=oracle.value(g);if(i<12)assert(exact(g)==target);bool different=false;for(auto a:legal(g)){Game z=g;apply(z,a);if(oracle.value(z)!=target)different=true;}if(!different)continue;discriminating++;Rng ro(912+i),rn(912+i);Act a=searchPolicy(g,ro,i%3,16,3),b=strongPolicy(g,rn,i%3,160);Game x=g,y=g;apply(x,a);apply(y,b);oldok+=oracle.value(x)==target;newok+=oracle.value(y)==target;if(oracle.value(x)!=target){cout<<"old mistake puzzle="<<i<<" turn="<<g.turn<<" pending="<<g.pending<<" score="<<score(g,0)<<":"<<score(g,1)<<" old="<<a.kind<<":"<<(a.card>=0?C[a.card].name:"pass/leader")<<" new="<<b.kind<<":"<<(b.card>=0?C[b.card].name:"pass/leader")<<"\n";}total++;}catch(Limit&){} }cout<<"discriminating puzzles="<<total<<" old_correct="<<oldok<<" new_correct="<<newok<<"\n";assert(newok==total);
 // Hidden-state invariance for the actual new policy, not just its rollout heuristic.
 auto ds=loadDecks(deckpath);auto ng=find_if(ds.begin(),ds.end(),[](auto d){return d.faction==1;});int hiddenChecks=0;for(int i=0;i<8;i++){Game g=start(ds[0],*ng,800+i);for(int k=0;k<i*5&&g.winner<0;k++)apply(g,policy(g,gen,i%3));if(g.winner>=0)continue;Game h=g;determinize(h,g.turn,gen);Rng a(71),b(71);assert(strongPolicy(g,a,0,80)==strongPolicy(h,b,0,80));hiddenChecks++;}cout<<"new policy hidden-state invariance: "<<hiddenChecks<<" passed\n";}
#ifndef GWENT_NO_MAIN
int main(int argc,char**argv){setup();tests();if(argc<2||string(argv[1])=="audit"){audit(argc>2?argv[2]:"search_0.txt");return 0;}auto ds=loadDecks(argv[1]);int count=argc>2?stoi(argv[2]):10;int iters=argc>3?10*stoi(argv[3]):160;bool both=argc>4&&string(argv[4])=="both";uint64_t matchSeed=argc>5?stoull(argv[5]):940100;auto ng=find_if(ds.begin(),ds.end(),[](auto d){return d.faction==1;});auto t=chrono::steady_clock::now();for(int side=0;side<(both?1:2);side++){int wins=0;for(int i=0;i<count;i++){Game g=start(ds[0],*ng,matchSeed+i*1000003ULL);auto inv=inventory(g);Rng r(941+i);while(g.winner<0){if(g.steps>500)throw runtime_error("stalled");Act a=(both||g.turn==side)?strongPolicy(g,r,i%3,iters):searchPolicy(g,r,i%3,16,3);apply(g,a);assert(inventory(g)==inv);}wins+=g.winner==side;}cout<<(both?"both_new_north=":"new_side=")<<side<<" wins="<<wins<<" games="<<count<<endl;}cout<<"seconds="<<chrono::duration<double>(chrono::steady_clock::now()-t).count()<<endl;}

#endif
