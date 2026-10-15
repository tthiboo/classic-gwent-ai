#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
ResponseModel learnedResponse(int axis,int mode){ResponseModel m;m.enabled=true;array<double,3> likelihood{.4,.1,.1};likelihood[mode]=.9;for(int n=0;n<10;n++)m.habits[axis].observe(likelihood);return m;}
int main(){setup();initClassic();originals=loadDecks("decks.txt");
 for(int axis=0;axis<4;axis++){
  ResponseBelief b;b.observe({.4,.9,.1});assert(!b.active());for(int n=0;n<9;n++)b.observe({.4,.9,.1});assert(b.active()&&b.weights()[1]>b.weights()[2]);
  for(int n=0;n<10;n++)b.observe({.4,.1,.9});assert(b.active()&&b.weights()[2]>b.weights()[1]);auto held=b.evidence;b.observe({.5,.5,.5});b.observe({.1,0,.9});assert(b.evidence==held);
 }
 for(int f=0;f<5;f++)for(int viewer=0;viewer<2;viewer++){
  auto own=preset(f,factionLeaders[f][0]),enemy=preset((f+1)%5,factionLeaders[(f+1)%5][0]);auto g=viewer?start(enemy,own,220731+f):start(own,enemy,220731+f);g.turn=1-viewer;
  auto moves=legal(g);auto a=*find_if(moves.begin(),moves.end(),[](auto x){return x.kind==0;});auto after=g;apply(after,a);
  memory={};auto rngBefore=g.rng.z;observeResponseDecision(g,after,a,viewer,own,enemy.cards.size(),9182);auto first=memory;
  auto hidden=g;hidden.p[1-viewer].hand.assign(hidden.p[1-viewer].hand.size(),-999);hidden.p[1-viewer].deck.assign(hidden.p[1-viewer].deck.size(),-997);reverse(hidden.p[viewer].deck.begin(),hidden.p[viewer].deck.end());hidden.rng=Rng(987);
  memory={};observeResponseDecision(hidden,after,a,viewer,own,enemy.cards.size(),9182);
  for(int axis=0;axis<4;axis++)assert(memory.responses[enemy.faction].habits[axis].evidence==first.responses[enemy.faction].habits[axis].evidence);assert(g.rng.z==rngBefore);
  memory={};g.p[viewer].passed=true;observeResponseDecision(g,after,a,viewer,own,enemy.cards.size(),9182);for(auto b:memory.responses[enemy.faction].habits)assert(!b.observations);
 }
 auto own=preset(0,0),enemy=preset(1,3);auto g=start(own,enemy,7812);g.turn=0;auto obs=observation(g,0);auto lib=publicDeckHypotheses(obs,own,enemy.cards.size(),912);auto h=compatible(obs,0,own,lib);
 memory={};decisionLearning={};ResponseModel cold;cold.enabled=true;
 auto base=parallelRoot(obs,h,721,0,{1,1,1},{128,0,18},true,1,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true);
 auto inactive=parallelRoot(obs,h,721,0,{1,1,1},{128,0,18},true,3,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true,&cold);
 assert(base.chosen==inactive.chosen&&base.iterations==inactive.iterations);for(int i=0;i<int(base.root.size());i++)assert(base.root[i].value==inactive.root[i].value&&base.root[i].visits==inactive.root[i].visits);
 for(int axis=0;axis<4;axis++)for(int mode=1;mode<3;mode++){
  auto m=learnedResponse(axis,mode);auto replay=encodeReplay(g,0,own,lib,721,0,{1,1,1},{1,0,0},true,false,true,&m);auto read=decodeReplay(replay);assert(read.response.enabled&&read.response.habits[axis].evidence==m.habits[axis].evidence);
  auto poisoned=g;poisoned.p[1].hand.assign(poisoned.p[1].hand.size(),-888);poisoned.p[1].deck.assign(poisoned.p[1].deck.size(),-777);reverse(poisoned.p[0].deck.begin(),poisoned.p[0].deck.end());
  auto first=parallelRoot(g,h,721,0,{1,1,1},{128,0,18},true,1,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true,&m);
  auto second=parallelRoot(poisoned,h,721,0,{1,1,1},{128,0,18},true,3,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true,&m);
  assert(first.chosen==second.chosen&&first.iterations==second.iterations);for(int i=0;i<int(first.root.size());i++)assert(first.root[i].value==second.root[i].value&&first.root[i].visits==second.root[i].visits);
  memory={};memory.responses[1]=m;memory.path="/tmp/gwent-response-isolated-memory.txt";memory.save();AdaptiveMemory restored;restored.load(memory.path);assert(restored.responses[1].habits[axis].evidence==m.habits[axis].evidence);remove(memory.path.c_str());
 }
 // All active models retain forced, exact and tactical safety exclusions.
 for(int axis=0;axis<4;axis++){
  auto m=learnedResponse(axis,1);Game forced;forced.p[0].used=forced.p[1].used=true;forced.p[0].hand={SCORCH,id("Vesemir"),id("Geralt of Rivia")};forced.p[0].board={{id("Black Infantry Archer"),1}};forced.p[1].board={{id("Geralt of Rivia"),0}};forced.p[1].wins=1;
  for(int n=0;n<20;n++){Rng r(91+n);auto a=responseOpponentReply(forced,r,0,0,&m);assert(a.kind!=3&&!selfOnlyScorch(forced,a));}
 }
 cout<<"PASS: four A-to-B belief reversals, one-observation uncertainty, 10 public/hidden observer contexts, inactive exact identity, eight replay/memory round trips and hidden/worker search comparisons, 80 forced tactical replies\n";
}
