#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
struct Rng{uint64_t z; Rng(uint64_t x=1):z(x){} uint64_t next(){z+=0x9e3779b97f4a7c15ULL;uint64_t x=z;x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;x=(x^(x>>27))*0x94d049bb133111ebULL;return x^(x>>31);} int n(int m){if(m<=1)return 0;uint64_t bound=m,threshold=-bound%bound,x;do{x=next();}while(x<threshold);return x%bound;} double u(){return (next()>>11)*0x1.0p-53;} template<class T> void shuffle(vector<T>&v){for(int i=(int)v.size()-1;i>0;i--)swap(v[i],v[n(i+1)]);}};
struct Card{string name;int p,row,flags,kind,weather;int group=-1,transform=-1,faction=-1,limit=1,collectible=1;};vector<Card> C;map<string,int> IDS;
void add(string s,int p,int r,int f,int k,int m){IDS[s]=C.size();C.push_back({s,p,r,f,k,m});}
#include "cards.inc"
int id(string s){return IDS.at(s);} const int HERO=1,SPY=2,MED=4,BOND=8,MORALE=16,DRAGON=32,UHORN=64,MUSTER=128,COW=256, BERSERK=512, MARDROEM=1024, KAMBI=2048,RSCORCH=4096,SSCORCH=8192,GSCORCH=16384;
int DEC,SCORCH,HORN,CLEAR,DRAG,BOVINE,DARK;
int MARD=-1,HEMDALL=-1;
struct Unit{int id,row;};
struct Player{vector<int> hand,deck,grave,known;vector<Unit> board;int faction=0,leader=0,wins=0,lhorn=0;bool used=false,passed=false;int drawn=0,spies=0,revives=0;};
struct Game{array<Player,2> p;vector<pair<int,int>> weather;int turn=0,pending=-1,round=0,winner=-1,steps=0;bool emhyrSpecial=false,heroThreshold=true;int pendingKind=0,remaining=0,openingTurn=0;bool opening=false;vector<pair<int,int>> openingQueue;map<int,int> changes;Rng rng;Game(){p[1].faction=1;}};
struct Deck{string name;int faction,leader;vector<int> cards;};
struct Act{int kind=0,card=-1,target=-1,row=-1;bool operator==(const Act&o)const{return kind==o.kind&&card==o.card&&target==o.target&&row==o.row;}};
// Action kinds: 0 play, 1 leader, 2 revive choice, 3 pass.
void erase1(vector<int>&v,int x){auto i=find(v.begin(),v.end(),x);if(i==v.end())throw runtime_error("missing card");v.erase(i);}
void consume(Player&p,int x){erase1(p.hand,x);auto k=find(p.known.begin(),p.known.end(),x);if(k!=p.known.end())p.known.erase(k);}
int weatherMask(const Game&g){int m=0;for(auto x:g.weather)m|=C[x.second].weather;return m;}
vector<int> powers(const Game&g,int s){auto &pl=g.p[s];int w=weatherMask(g);array<int,3> morale{},horn{};map<pair<int,int>,int> bonds;for(auto u:pl.board){auto c=C[u.id];if(c.kind){if(u.id==HORN)horn[u.row]++;continue;}if(c.flags&MORALE)morale[u.row]++;if(c.flags&UHORN)horn[u.row]++;if(c.flags&BOND)bonds[{c.group>=0?c.group:u.id,u.row}]++;}for(int r=0;r<3;r++)if(pl.lhorn&(1<<r))horn[r]++;
 vector<int> out;for(auto u:pl.board){auto c=C[u.id];if(c.kind){out.push_back(0);continue;}if(c.flags&HERO){out.push_back(c.p);continue;}bool bran=pl.leader==21&&!pl.used;int v=(w&(1<<u.row))?(bran?(c.p+1)/2:1):c.p;if(c.flags&SPY){bool boost=false;for(auto& p:g.p)boost|=p.leader==14&&!p.used;if(boost)v*=2;}if(c.flags&BOND)v*=bonds[{c.group>=0?c.group:u.id,u.row}];v+=morale[u.row]-!!(c.flags&MORALE);if(horn[u.row]-!!(c.flags&UHORN)>0)v*=2;out.push_back(v);}return out;}
int score(const Game&g,int s){auto p=powers(g,s);return accumulate(p.begin(),p.end(),0);}
bool ahead(const Game&g,int s){int d=score(g,s)-score(g,1-s);return d>0||(d==0&&g.p[s].faction==1&&g.p[1-s].faction!=1);}
void draw(Game&g,int s,int n){for(int i=0;i<n&&!g.p[s].deck.empty();i++){g.p[s].hand.push_back(g.p[s].deck.back());g.p[s].deck.pop_back();g.p[s].drawn++;}}
void clearWeather(Game&g){for(auto x:g.weather)g.p[x.first].grave.push_back(x.second);g.weather.clear();}
void killIndices(Game&g,int s,const vector<int>&idx){vector<int> born;for(auto it=idx.rbegin();it!=idx.rend();++it){int c=g.p[s].board[*it].id;g.p[s].grave.push_back(c);if(C[c].flags&COW)born.push_back(BOVINE);if(C[c].flags&KAMBI)born.push_back(HEMDALL);g.p[s].board.erase(g.p[s].board.begin()+*it);}for(int c:born){g.p[s].board.push_back({c,C[c].row});g.changes[c]++;}}
void scorch(Game&g,int target=-1,int row=-1){array<vector<int>,2> v={powers(g,0),powers(g,1)};int mx=-1,total=0;if(target>=0){for(int i=0;i<(int)g.p[target].board.size();i++){auto u=g.p[target].board[i];if(u.row==row&&!C[u.id].kind&&(g.heroThreshold||!(C[u.id].flags&HERO)))total+=v[target][i];}if(total<10)return;}
 for(int s=0;s<2;s++)if(target<0||target==s)for(int i=0;i<(int)g.p[s].board.size();i++){auto u=g.p[s].board[i];if(!C[u.id].kind&&!(C[u.id].flags&HERO)&&(row<0||u.row==row))mx=max(mx,v[s][i]);}
 if(mx<0)return;array<vector<int>,2> dead;for(int s=0;s<2;s++)if(target<0||target==s)for(int i=0;i<(int)g.p[s].board.size();i++){auto u=g.p[s].board[i];if(!C[u.id].kind&&!(C[u.id].flags&HERO)&&(row<0||u.row==row)&&v[s][i]==mx)dead[s].push_back(i);}for(int s=0;s<2;s++)killIndices(g,s,dead[s]);}
bool revivable(int x){return !C[x].kind&&!(C[x].flags&HERO);}
bool hasRevive(const Game&g,int s){for(int c:g.p[s].grave)if(revivable(c))return true;return false;}
void place(Game&,int,int,int);
void advanceOpening(Game& g){while(!g.openingQueue.empty()){auto [s,c]=g.openingQueue.front();g.openingQueue.erase(g.openingQueue.begin());g.turn=s;place(g,s,c,C[c].row==3?0:C[c].row);if(g.pending>=0)return;}g.opening=false;g.turn=g.openingTurn;}
void finish(Game&g){
 if(g.pending>=0)return;
 if(g.opening){advanceOpening(g);return;}
 if(g.p[0].passed&&g.p[1].passed){
  bool a=ahead(g,0),b=ahead(g,1);int winner=a?0:b?1:-1;
  if(winner<0){g.p[0].wins++;g.p[1].wins++;}else g.p[winner].wins++;
  if(g.p[0].wins>=2||g.p[1].wins>=2){g.winner=g.p[0].wins>=2?(g.p[1].wins>=2?2:0):1;return;}
  array<vector<Unit>,2> keep;
  for(int side=0;side<2;side++){
   vector<int> candidates;if(g.p[side].faction==2)for(int i=0;i<(int)g.p[side].board.size();i++)if(revivable(g.p[side].board[i].id))candidates.push_back(i);
   int retained=candidates.empty()?-1:candidates[g.rng.n(candidates.size())];
   for(int i=0;i<(int)g.p[side].board.size();i++){auto u=g.p[side].board[i];if(i==retained){keep[side].push_back(u);continue;}g.p[side].grave.push_back(u.id);if(C[u.id].flags&(COW|KAMBI)){int token=(C[u.id].flags&COW)?BOVINE:HEMDALL;keep[side].push_back({token,0});g.changes[token]++;}}
   g.p[side].board=keep[side];g.p[side].passed=false;g.p[side].lhorn=0;
  }
  clearWeather(g);g.round++;if(winner>=0&&g.p[winner].faction==0)draw(g,winner,1);
  g.turn=winner>=0?winner:g.rng.n(2);g.openingTurn=g.turn;
  if(g.round==2){for(int side=0;side<2;side++)if(g.p[side].faction==4){for(int k=0;k<2;k++){vector<int> pool;for(int c:g.p[side].grave)if(revivable(c))pool.push_back(c);if(pool.empty())break;int c=pool[g.rng.n(pool.size())];erase1(g.p[side].grave,c);g.openingQueue.push_back({side,c});}}if(!g.openingQueue.empty()){g.opening=true;advanceOpening(g);}}
  return;
 }
 if(!g.p[1-g.turn].passed)g.turn=1-g.turn;
}
void transformRow(Game&g,int side,int row){bool active=false;for(auto u:g.p[side].board)if(u.row==row&&(u.id==MARD||(C[u.id].flags&MARDROEM)))active=true;if(!active)return;for(auto &u:g.p[side].board)if(u.row==row&&(C[u.id].flags&BERSERK)){int old=u.id;u.id=C[old].transform;g.changes[old]--;g.changes[u.id]++;}}

void place(Game&g,int s,int card,int row){
 auto c=C[card];int side=(c.flags&SPY)?1-s:s;if(row<0)row=c.row;if(row==3)row=0;g.p[side].board.push_back({card,row});
 if(c.flags&SPY){g.p[s].spies++;draw(g,s,2);}if(c.flags&DRAGON)scorch(g,1-s,0);if(c.flags&RSCORCH)scorch(g,1-s,1);if(c.flags&SSCORCH)scorch(g,1-s,2);if(c.flags&GSCORCH)scorch(g);
 if(c.flags&MUSTER){int group=c.group;vector<int> found;for(auto* zone:{&g.p[s].hand,&g.p[s].deck})for(int i=(int)zone->size()-1;i>=0;i--){int x=(*zone)[i];bool match=card==id("Arachas Behemoth")?C[x].name=="Arachas":card==id("Gaunter O'Dimm")?x==DARK:(group>=0&&C[x].group==group);if(match){found.push_back(x);zone->erase(zone->begin()+i);}}for(int x:found){auto it=find(g.p[s].known.begin(),g.p[s].known.end(),x);if(it!=g.p[s].known.end())g.p[s].known.erase(it);g.p[s].board.push_back({x,C[x].row==3?row:C[x].row});}}
 for(int r=0;r<3;r++)transformRow(g,side,r);
 if((c.flags&MED)&&hasRevive(g,s)){
  bool random=false;for(auto& p:g.p)random|=p.leader==9&&!p.used;
  if(random){vector<int> pool;for(int x:g.p[s].grave)if(revivable(x))pool.push_back(x);int x=pool[g.rng.n(pool.size())];erase1(g.p[s].grave,x);g.p[s].revives++;place(g,s,x,C[x].row==3?0:C[x].row);}
  else{g.pending=s;g.pendingKind=0;}
 }
}

vector<Act> legal(const Game&g){int s=g.turn;vector<Act>a;if(g.winner>=0)return a;if(g.pending>=0&&g.pendingKind){set<int> seen;auto& zone=g.pendingKind==1?g.p[s].hand:g.p[s].deck;for(int c:zone)if(seen.insert(c).second)a.push_back({g.pendingKind==1?4:5,c});return a;}if(g.pending>=0){set<int>seen;for(int c:g.p[s].grave)if(revivable(c)&&seen.insert(c).second){if(C[c].row==3){a.push_back({2,c,-1,0});a.push_back({2,c,-1,1});}else a.push_back({2,c,-1,C[c].row});}return a;}
 set<int>seen;for(int c:g.p[s].hand)if(seen.insert(c).second){if(c==DEC){for(int i=0;i<(int)g.p[s].board.size();i++)if(revivable(g.p[s].board[i].id))a.push_back({0,c,i,-1});}else if(c==MARD){for(int r=0;r<3;r++)a.push_back({0,c,-1,r});}else if(c==HORN){for(int r=0;r<3;r++){bool has=false;for(auto u:g.p[s].board)if(u.id==HORN&&u.row==r)has=true;if(!has)a.push_back({0,c,-1,r});}}else if(C[c].row==3){a.push_back({0,c,-1,0});a.push_back({0,c,-1,1});}else a.push_back({0,c,-1,C[c].row});}
 if(!g.p[s].used){int l=g.p[s].leader;
 if(l==3||l==13){set<int>x;for(int c:g.p[l==3?1-s:s].grave)if(revivable(c)&&x.insert(c).second)a.push_back({1,-1,c});}
 else if(l==1){if(!g.weather.empty())a.push_back({1});}
 else if(l==5||l==6||l==11||l==18){set<int> x;int mask=l==5?4:l==6?2:1;for(int c:g.p[s].deck)if(C[c].weather&&(l==11||C[c].weather==mask)&&x.insert(c).second)a.push_back({1,-1,c});}
 else if(l==12){if(g.p[s].hand.size()>=2&&!g.p[s].deck.empty())a.push_back({1});}
 else if(l!=4&&l!=9&&l!=14&&l!=15&&l!=21)a.push_back({1});
 }

 a.push_back({3});return a;}
void apply(Game&g,Act a){int s=g.turn;g.steps++;
 if(a.kind==4){consume(g.p[s],a.card);g.p[s].grave.push_back(a.card);if(--g.remaining==0)g.pendingKind=2;return;}
 if(a.kind==5){erase1(g.p[s].deck,a.card);g.p[s].hand.push_back(a.card);g.pending=-1;g.pendingKind=0;finish(g);return;}
 if(a.kind==3){g.p[s].passed=true;finish(g);return;}if(a.kind==2){erase1(g.p[s].grave,a.card);g.pending=-1;g.p[s].revives++;place(g,s,a.card,a.row);finish(g);return;}if(a.kind==1){g.p[s].used=true;int l=g.p[s].leader;
 if(l==0)g.p[s].lhorn|=4;else if(l==10)g.p[s].lhorn|=1;else if(l==16)g.p[s].lhorn|=2;
 else if(l==1)clearWeather(g);else if(l==2)scorch(g,1-s,1);else if(l==7)scorch(g,1-s,2);else if(l==17)scorch(g,1-s,0);
 else if(l==3||l==13){erase1(g.p[l==3?1-s:s].grave,a.target);g.p[s].hand.push_back(a.target);g.p[s].known.push_back(a.target);}
 else if(l==5||l==6||l==11||l==18){erase1(g.p[s].deck,a.target);g.weather.push_back({s,a.target});}
 else if(l==8){auto pool=g.p[1-s].hand;g.rng.shuffle(pool);map<int,int> revealed;for(int i=0;i<min(3,(int)pool.size());i++)revealed[pool[i]]++;for(auto [c,n]:revealed)while(count(g.p[1-s].known.begin(),g.p[1-s].known.end(),c)<n)g.p[1-s].known.push_back(c);}
 else if(l==12){g.pending=s;g.pendingKind=1;g.remaining=2;}
 else if(l==20){for(auto& p:g.p){p.deck.insert(p.deck.end(),p.grave.begin(),p.grave.end());p.grave.clear();g.rng.shuffle(p.deck);}}
 else if(l==19){vector<int> agile;for(int i=0;i<(int)g.p[s].board.size();i++)if(C[g.p[s].board[i].id].row==3)agile.push_back(i);int best=score(g,s),bestmask=0;for(int mask=0;mask<(1<<(int)agile.size());mask++){Game z=g;for(int i=0;i<(int)agile.size();i++)z.p[s].board[agile[i]].row=(mask>>i)&1;int v=score(z,s);if(v>best){best=v;bestmask=mask;}}if(best>score(g,s))for(int i=0;i<(int)agile.size();i++)g.p[s].board[agile[i]].row=(bestmask>>i)&1;}
 finish(g);return;}

 int c=a.card;consume(g.p[s],c);if(!C[c].kind){place(g,s,c,a.row);}else if(c==DEC){auto u=g.p[s].board[a.target];g.p[s].hand.push_back(u.id);g.p[s].known.push_back(u.id);g.p[s].board[a.target]={DEC,u.row};}else if(c==SCORCH){scorch(g);g.p[s].grave.push_back(c);}else if(c==CLEAR){clearWeather(g);g.p[s].grave.push_back(c);}else if(c==HORN||c==MARD){g.p[s].board.push_back({c,a.row});transformRow(g,s,a.row);}else if(C[c].weather)g.weather.push_back({s,c});finish(g);}
void determinize(Game&g,int observer,Rng&r){sort(g.p[observer].deck.begin(),g.p[observer].deck.end());r.shuffle(g.p[observer].deck);int o=1-observer;vector<int>pool=g.p[o].hand;pool.insert(pool.end(),g.p[o].deck.begin(),g.p[o].deck.end());for(int c:g.p[o].known)erase1(pool,c);sort(pool.begin(),pool.end());r.shuffle(pool);int need=g.p[o].hand.size()-g.p[o].known.size();g.p[o].hand=g.p[o].known;for(int i=0;i<need;i++){g.p[o].hand.push_back(pool.back());pool.pop_back();}g.p[o].deck=pool;g.rng=Rng(r.next());}
double quality(int c){if(C[c].flags&SPY)return 12;if(C[c].flags&MED)return 14;if(c==DEC)return 8;if(c==SCORCH)return 12;if(c==HORN)return 9;if(C[c].weather||c==CLEAR)return 5;return C[c].p;}
Act pendingChoice(const Game&g,Rng&r,double cv){auto acts=legal(g);double best=-1e9;Act chosen=acts.front();int s=g.turn;int old=score(g,s)-score(g,1-s);for(auto a:acts){Game z=g;apply(z,a);double val=score(z,s)-score(z,1-s)-old+cv*((int)z.p[s].hand.size()-(int)g.p[s].hand.size());if(z.pending>=0)val+=18;val+=r.u()*.05;if(val>best){best=val;chosen=a;}}return chosen;}
void finishPending(Game&g,Rng&r,double cv){int cap=0;while(g.pending>=0&&g.winner<0){if(++cap>30)throw runtime_error("medic loop");apply(g,pendingChoice(g,r,cv));}}
double scorchExposure(const Game&g,int s){auto p=powers(g,s);int mx=-1,loss=0;for(int i=0;i<(int)p.size();i++)if(revivable(g.p[s].board[i].id))mx=max(mx,p[i]);for(int i=0;i<(int)p.size();i++)if(revivable(g.p[s].board[i].id)&&p[i]==mx)loss+=p[i];return loss;}
struct Scored{Act a;double score;};
vector<Scored> rankActions(const Game&g,Rng&r,int style){int s=g.turn,o=1-s;double cv=style==0?20:style==1?15:25;auto acts=legal(g);if(g.pending>=0)return {{pendingChoice(g,r,cv),100}};Game blind=g;determinize(blind,s,r);int diff=score(g,s)-score(g,o);bool leading=ahead(g,s);bool forced=g.p[o].wins==1;bool finalround=g.p[s].wins==1&&forced;double urgency=finalround?1.2:forced?1.12:1.0;double oldRisk=scorchExposure(g,s);int handdiff=(int)g.p[s].hand.size()-(int)g.p[o].hand.size();vector<Scored>res;
 for(auto a:acts){if(a.kind==3){double val=-90;if(g.p[o].passed){if(leading)val=g.p[s].wins==1?100000:30;else val=forced?-100000:-65;}else if(leading){val=-24+0.65*diff-2*max(0,handdiff);if(g.p[s].hand.size()<=2)val+=10;}else if(!forced){double gap=-diff;val=-42+0.65*gap-3*handdiff;}res.push_back({a,val});continue;}
  Game z=blind;apply(z,a);finishPending(z,r,cv);if(z.winner>=0){res.push_back({a,z.winner==s?100000.0:-100000.0});continue;}int ndiff=score(z,s)-score(z,o);double gain=ndiff-diff;int dh=(int)z.p[s].hand.size()-(int)g.p[s].hand.size();double val=urgency*gain+cv*dh;
  if(a.kind==0&&a.card==DEC){int t=g.p[s].board[a.target].id;if((C[t].flags&SPY)&&!g.p[s].deck.empty())val+=min(2,(int)g.p[s].deck.size())*cv-cv-C[t].p;else if((C[t].flags&MED)&&hasRevive(g,s))val+=cv+5;else if(C[t].flags&DRAGON){Game zz=g;scorch(zz,o,0);val+=max(0,score(g,o)-score(zz,o))*.75;}}
  // Resource conservation: avoid spending top heroes and controls for tiny marginal gains.
  if(a.kind==0)val-=.12*quality(a.card);
  // Do not assume unseen Scorches are absent. This is a tunable bot heuristic, not a game fact.
  int seenSc=count(g.p[o].grave.begin(),g.p[o].grave.end(),SCORCH);double risk=style==1?.08:style==2?.28:.16;
  if(seenSc<3&&!g.p[o].passed)val-=risk*(scorchExposure(z,s)-oldRisk);
  if(a.kind==1&&g.p[s].leader==0&&gain<10)val-=10;
  if(g.p[o].passed){if(leading){if(!ahead(z,s))val=-100000;else{val=cv*dh-.02*max(0.0,gain);if(a.kind==0&&a.card==DEC)val+=quality(g.p[s].board[a.target].id)-quality(DEC);}}else if(ahead(z,s))val+=38;}
  // Cantrip-like actions need not be chosen from actual unseen draws: only hand counts enter this evaluation.
  val+=r.u()*(style==1?1.5:.15);res.push_back({a,val});}
 sort(res.begin(),res.end(),[](auto&a,auto&b){return a.score>b.score;});return res;}
Act policy(const Game&g,Rng&r,int style){return rankActions(g,r,style).front().a;}
int rollout(Game g,Rng&r,int style){while(g.winner<0){if(g.steps>500)throw runtime_error("too long");apply(g,policy(g,r,style));}return g.winner;}
Act searchPolicy(const Game&g,Rng&r,int style,int trials,int width){if(g.pending>=0)return pendingChoice(g,r,style==0?20:style==1?15:25);auto ranked=rankActions(g,r,style);if(ranked.size()==1||ranked.front().score>90000)return ranked.front().a;vector<Act> cand;for(int i=0;i<min(width,(int)ranked.size());i++)cand.push_back(ranked[i].a);bool havepass=false;for(auto a:cand)havepass|=a.kind==3;if(!havepass)cand.push_back({3});
 // Include hold/answer choices which an immediate-effect ranking tends to miss.
 for(auto q:ranked)if(q.a.kind==1||(q.a.kind==0&&(C[q.a.card].weather||q.a.card==CLEAR)))if(find(cand.begin(),cand.end(),q.a)==cand.end())cand.push_back(q.a);
 int s=g.turn;vector<double> wins(cand.size(),0);for(int t=0;t<trials;t++){Game det=g;determinize(det,s,r);uint64_t seed=r.next();for(int i=0;i<(int)cand.size();i++){Game z=det;apply(z,cand[i]);Rng rr(seed);int w=rollout(z,rr,(style+t)%3);wins[i]+=w==s?1:0;}}
 int b=0;for(int i=1;i<(int)cand.size();i++)if(wins[i]>wins[b])b=i;return cand[b];}
void mulligan(Game&g,int s){vector<int>returned;int dealt=g.p[s].drawn;for(int k=0;k<2;k++){int worst=-1;double val=1e9;map<int,int>h;for(int x:g.p[s].hand)h[x]++;for(int i=0;i<(int)g.p[s].hand.size();i++){int c=g.p[s].hand[i];double v=quality(c);if(C[c].flags&SPY)v=100;if(c==DEC)v=70;if(C[c].flags&MED)v=60;if(C[c].flags&BOND)v+=4*(h[c]-1);if(c==CLEAR&&g.p[1-s].leader==5)v=65;if(v<val){val=v;worst=i;}}if(worst<0||val>=60)break;returned.push_back(g.p[s].hand[worst]);g.p[s].hand.erase(g.p[s].hand.begin()+worst);draw(g,s,1);}g.p[s].deck.insert(g.p[s].deck.end(),returned.begin(),returned.end());g.rng.shuffle(g.p[s].deck);g.p[s].drawn=dealt;}
Game start(const Deck&n,const Deck&ng,uint64_t seed,bool special=false,bool threshold=true){Game g;g.p[0].faction=n.faction;g.p[1].faction=ng.faction;g.rng=Rng(seed);g.emhyrSpecial=special;g.heroThreshold=threshold;g.p[0].deck=n.cards;g.p[1].deck=ng.cards;g.p[0].leader=n.leader;g.p[1].leader=ng.leader;for(int s=0;s<2;s++){g.rng.shuffle(g.p[s].deck);draw(g,s,10+(g.p[s].leader==15&&g.p[1-s].leader!=4?1:0));}if(ng.leader==4||n.leader==4){g.p[0].used=true;g.p[1].used=true;}mulligan(g,0);mulligan(g,1);g.turn=g.rng.n(2);return g;}
map<int,int> inventory(const Game&g){map<int,int>m;for(int s=0;s<2;s++){for(auto*z:{&g.p[s].hand,&g.p[s].deck,&g.p[s].grave})for(int c:*z)m[c]++;for(auto u:g.p[s].board)m[u.id]++;}for(auto w:g.weather)m[w.second]++;for(auto [s,c]:g.openingQueue)m[c]++;for(auto [c,n]:g.changes)m[c]-=n;for(auto it=m.begin();it!=m.end();)if(it->second==0)it=m.erase(it);else ++it;return m;}
vector<Deck> loadDecks(string path){ifstream f(path);string line;vector<Deck>d;while(getline(f,line)){if(line.empty())continue;stringstream ss(line);string a,b,c,t;getline(ss,a,'|');getline(ss,b,'|');getline(ss,c,'|');getline(ss,t);Deck x{a,stoi(b),stoi(c),{}};stringstream ids(t);while(getline(ids,a,','))x.cards.push_back(stoi(a));int units=0,special=0;for(int c:x.cards){units+=!C[c].kind;special+=C[c].kind;}if(units<22||special>10)throw runtime_error("invalid deck "+x.name);d.push_back(x);}return d;}
void setup(){init_cards();DEC=id("Decoy");SCORCH=id("Scorch");HORN=id("Commander's Horn");CLEAR=id("Clear Weather");DRAG=id("Villentretenmerth");BOVINE=id("Bovine Defense Force");DARK=id("Gaunter O'Dimm: Darkness");}
void tests(){int imp=id("Impera Brigade Guard"),cat=id("Catapult"),hero=id("Geralt of Rivia"),frost=id("Biting Frost"),med=id("Dun Banner Medic"),yen=id("Yennefer of Vengerberg"),spy=id("Thaler");Game g;for(int k=0;k<4;k++)g.p[1].board.push_back({imp,0});assert(score(g,1)==48);g.weather.push_back({0,frost});assert(score(g,1)==16);g.p[1].board.push_back({HORN,0});assert(score(g,1)==32);g.p[0].hand={DRAG};apply(g,{0,DRAG,-1,0});assert(score(g,1)==0);assert(count(g.p[1].grave.begin(),g.p[1].grave.end(),imp)==4);
 g=Game();g.p[0].board={{cat,2},{cat,2},{hero,0}};g.p[0].lhorn=4;assert(score(g,0)==79);g.p[1].hand={SCORCH};g.turn=1;apply(g,{0,SCORCH});assert(score(g,0)==15);
 g=Game();g.p[0].hand={yen};g.p[0].grave={med,spy};g.p[0].deck={hero,cat};apply(g,{0,yen,-1,1});assert(g.pending==0&&g.turn==0);apply(g,{2,med,-1,2});assert(g.pending==0);apply(g,{2,spy,-1,2});assert(g.pending==-1&&g.turn==1&&g.p[0].hand.size()==2&&g.p[1].board[0].id==spy);
 g=Game();g.p[0].board={{hero,0}};g.p[0].hand={DEC};auto a=legal(g);for(auto x:a)assert(x.card!=DEC);
 g=Game();g.p[0].board={{DRAG,0}};g.p[0].hand={DEC};apply(g,{0,DEC,0});assert(g.p[0].known.size()==1&&g.p[0].hand[0]==DRAG&&score(g,0)==0);
 g=Game();g.p[0].passed=true;g.turn=1;apply(g,{3});assert(g.p[1].wins==1&&g.turn==1);
 g=Game();g.p[0].board={{hero,0}};g.p[0].deck={cat};g.p[0].passed=g.p[1].passed=true;finish(g);assert(g.p[0].wins==1&&g.p[0].hand.size()==1&&g.turn==0);
 g=Game();g.p[0].hand={DARK};g.p[0].deck={DARK,DARK,cat};apply(g,{0,DARK,-1,1});assert(g.p[0].board.size()==3&&g.p[0].deck.size()==1);
 // A hero contributes to the threshold but is not destroyed.
 g=Game();g.weather={{0,frost}};g.p[1].board={{hero,0},{imp,0}};scorch(g,1,0);assert(g.p[1].board.size()==1&&g.p[1].board[0].id==hero);
 cerr<<"mechanics tests passed\n";}
long long nodes=0;
int exact(Game g){nodes++;if(g.winner>=0)return g.winner==2?0:g.winner==0?1:-1;auto a=legal(g);int best=g.turn==0?-2:2;int who=g.turn;for(auto x:a){Game z=g;apply(z,x);int v=exact(z);best=who==0?max(best,v):min(best,v);if((who==0&&best==1)||(who==1&&best==-1))break;}return best;}
void mini(){for(int f=0;f<=2;f++)for(int c=0;c<=2;c++){Game g;g.p[0].wins=g.p[1].wins=1;g.p[0].used=g.p[1].used=true;g.p[0].lhorn=4;g.p[0].board={{id("Catapult"),2},{id("Catapult"),2},{id("Geralt of Rivia"),0},{id("Cirilla Fiona Elen Riannon"),0}};g.p[1].board={{id("Geralt of Rivia"),0},{id("Cirilla Fiona Elen Riannon"),0},{HORN,0}};for(int i=0;i<4;i++)g.p[1].board.push_back({id("Impera Brigade Guard"),0});g.p[0].hand=vector<int>(f,id("Biting Frost"));g.p[1].hand=vector<int>(c,CLEAR);nodes=0;int r=exact(g);cout<<"frost="<<f<<" clear="<<c<<" winner="<<(r==1?"North":"Nilfgaard")<<" nodes="<<nodes<<"\n";assert((r==1)==(f>c));int correct=0;for(int st=0;st<3;st++){Rng rr(991+st);Act chosen=searchPolicy(g,rr,st,16,3);Game z=g;apply(z,chosen);correct+=exact(z)==r;}cout<<"  rollout optimal first moves: "<<correct<<"/3\n";}}
int main(int argc,char**argv){setup();tests();if(argc>=2&&string(argv[1])=="mini"){mini();return 0;}if(argc<3)return 0;auto decks=loadDecks(argv[1]);string mode=argv[2];int games=argc>3?stoi(argv[3]):40;uint64_t seed=argc>4?stoull(argv[4]):12345;int trials=argc>5?stoi(argv[5]):0;bool special=argc>6?stoi(argv[6]):false;int styleBase=argc>7?stoi(argv[7]):-1;bool verify=mode=="verify";int total=0;
 for(auto &n:decks)if(n.faction==0)for(auto &ng:decks)if(ng.faction==1){int wins=0;double sn=0,sg=0,rn=0,rg=0;for(int i=0;i<games;i++){uint64_t ss=seed+uint64_t(i)*1000003;Game g=start(n,ng,ss,special);auto inv=inventory(g);
if(verify && i==0){Game altered=g;int observer=g.turn,o=1-observer;Rng mix(91);vector<int> pool=altered.p[o].hand;pool.insert(pool.end(),altered.p[o].deck.begin(),altered.p[o].deck.end());mix.shuffle(pool);int h=altered.p[o].hand.size();altered.p[o].hand.assign(pool.begin(),pool.begin()+h);altered.p[o].deck.assign(pool.begin()+h,pool.end());reverse(altered.p[observer].deck.begin(),altered.p[observer].deck.end());Rng ra(777),rb(777);if(!(policy(g,ra,0)==policy(altered,rb,0)))throw runtime_error("hidden information leak");}
Rng r(ss^0xbeef9876ULL);int style=styleBase>=0?styleBase:i%3;while(g.winner<0){if(g.steps>500)throw runtime_error("stalled match");int actingStyle=styleBase>=0?style:(g.turn==0?i%3:(i/3)%3);Act a=trials?searchPolicy(g,r,actingStyle,trials,3):policy(g,r,actingStyle);if(mode=="trace")cerr<<"R"<<g.round+1<<" P"<<g.turn<<" score "<<score(g,0)<<":"<<score(g,1)<<" hands "<<g.p[0].hand.size()<<":"<<g.p[1].hand.size()<<" action "<<a.kind<<" "<<(a.card>=0?C[a.card].name:to_string(a.target))<<"\n";
apply(g,a);if(verify&&inventory(g)!=inv)throw runtime_error("conservation failure");}wins+=g.winner==0;sn+=g.p[0].spies;sg+=g.p[1].spies;rn+=g.p[0].revives;rg+=g.p[1].revives;total++;}
 cout<<n.name<<'\t'<<ng.name<<'\t'<<wins<<'\t'<<games<<'\t'<<sn/games<<'\t'<<sg/games<<'\t'<<rn/games<<'\t'<<rg/games<<'\n';cout.flush();}cerr<<"matches "<<total<<"\n";return 0;}
