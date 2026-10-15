#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
ReportV9 equalReport(Act first,Act second,int visits=20,double value=16){ReportV9 r;r.chosen=first;r.root={{first,.6,value,visits},{second,.4,value,visits}};return r;}
int main(){setup();initClassic();originals=loadDecks("decks.txt");memory={};decisionLearning={};moveLearning={};
 int hero=id("Geralt of Rivia"),cheap=id("Zoltan Chivay"),frost=id("Biting Frost"),archer=id("Black Infantry Archer");Act big{0,hero,-1,0},small{0,cheap,-1,0};
 Game g;g.turn=0;g.p[0].hand={hero,cheap,frost};g.p[1].hand=vector<int>(8,-1);g.p[0].used=g.p[1].used=true;
 auto pressure=equalReport(small,big);chooseCharacterTie(g,pressure,1);assert(pressure.chosen==big&&pressure.characterTieChanged);
 auto reserve=equalReport(big,small);chooseCharacterTie(g,reserve,2);assert(reserve.chosen==small&&reserve.characterTieChanged);
 auto control=g;control.p[1].board={{archer,1},{archer,1}};Act weather{0,frost,-1,-1};control.p[1].board={{id("Vesemir"),0},{id("Vesemir"),0}};
 auto counter=equalReport(big,weather);chooseCharacterTie(control,counter,0);assert(counter.chosen==weather&&counter.characterTieChanged);
 auto collateral=g;collateral.p[0].board={{id("Vesemir"),0},{id("Vesemir"),0}};collateral.p[1].board={{id("Vesemir"),0}};
 auto harmful=equalReport(big,weather);chooseCharacterTie(collateral,harmful,0);assert(harmful.chosen==big&&!harmful.characterTieChanged);
 // Evidence and simulations always retain precedence; even a small measured
 // difference cannot be overwritten by the character's preferred objective.
 for(int style=0;style<3;style++)for(int reason=0;reason<7;reason++){
  auto state=g;auto r=equalReport(big,small);
  if(reason==0)r.root[1].value-=.5;
  if(reason==1)r.root[1].visits++;
  if(reason==2)r=equalReport(big,small,7,6);
  if(reason==3)r=equalReport(big,small,20,0);
  if(reason==4)state.p[1].passed=true;
  if(reason==5)state.pending=0;
  if(reason==6)state.opening=true;
  chooseCharacterTie(state,r,style);assert(r.chosen==big&&!r.characterTieChanged);
 }
 auto pass=equalReport({3},small);chooseCharacterTie(g,pass,1);assert(pass.chosen.kind==3&&!pass.characterTieChanged);
 auto leader=equalReport({1},small);chooseCharacterTie(g,leader,2);assert(leader.chosen.kind==1&&!leader.characterTieChanged);
 auto illegal=equalReport(big,{0,id("Triss Merigold"),-1,0});chooseCharacterTie(g,illegal,2);assert(illegal.chosen==big);
 // Same mechanics and cost cannot be made "different" by artwork/card name.
 auto equivalent=g;int twin=id("Cirilla Fiona Elen Riannon");equivalent.p[0].hand.push_back(twin);Act similar{0,twin,-1,0};
 auto unchanged=equalReport(big,similar);chooseCharacterTie(equivalent,unchanged,1);assert(unchanged.chosen==big);
 auto beforeKey=informationKeyV9(g,0,true);auto beforeRng=g.rng.z;
 auto poisoned=g;fill(poisoned.p[1].hand.begin(),poisoned.p[1].hand.end(),-999);poisoned.p[0].deck={cheap,hero};g.p[0].deck={hero,cheap};
 for(int style=0;style<3;style++){assert(characterPreference(g,big,style)==characterPreference(poisoned,big,style));assert(characterPreference(g,small,style)==characterPreference(poisoned,small,style));}
 assert(g.rng.z==beforeRng);g.p[0].deck.clear();assert(informationKeyV9(g,0,true)==beforeKey);
 // Actual tree allocation and outcome evidence are unaffected by the secondary
 // selector. Compare exact iteration-limited search before/after the selector.
 for(int f=0;f<5;f++)for(int side=0;side<2;side++){
  auto own=preparedDeck(f,(f+1)%5,f%3,factionLeaders[f][0]),other=preparedDeck((f+1)%5,f,0,factionLeaders[(f+1)%5][0]);auto game=side?start(other,own,582391+f):start(own,other,582391+f);game.turn=side;
  auto obs=observation(game,side);auto lib=publicDeckHypotheses(obs,own,other.cards.size(),81176);auto h=compatible(obs,side,own,lib);
  auto a=parallelRoot(obs,h,53812,f%3,{1,1,1},{64,0,18},true,1,true,{1,0,0},false,false,true,true);auto b=a;chooseCharacterTie(obs,b,f%3);
  assert(a.iterations==b.iterations&&a.nodes==b.nodes&&a.root.size()==b.root.size());
  for(int j=0;j<int(a.root.size());j++)assert(a.root[j].value==b.root[j].value&&a.root[j].visits==b.root[j].visits&&a.root[j].prior==b.root[j].prior);
  auto moves=legal(game);assert(find(moves.begin(),moves.end(),b.chosen)!=moves.end());
 }
 cout<<"PASS: distinct pressure/reserve/control objectives, 21 exclusion cases, pass/leader protection, legality, semantic identity, hidden/order invariance, pure selector and 10 preserved full-search contexts\n";
}
