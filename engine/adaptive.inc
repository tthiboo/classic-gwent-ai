#include <fstream>
#include "opponent_model.inc"
#include "preparation_history.inc"
// This layer receives public evidence and the bot's own deck, never the human deck list.
struct AdaptiveMemory {
 struct CharacterHistory {int last=-1,streak=0;};
 CharacterHistory characters[5][5];
 PreparationHistory preparation[5];OpponentTempo tempo[5];
 StyleBelief behaviour[5];RoundPlanBelief roundPlans[5];ResponseModel responses[5];ResponseWindow responseWindows[5];
 string path;double games[5][5][3]{},wins[5][5][3]{};int seen[5][256]{};int mistakes[2]{},good[2]{};
 void load(string p){path=p;ifstream in(p);string version;if(!(in>>version)||version!="GWENT_ADAPT_2")return;for(int own=0;own<5;own++)for(int f=0;f<5;f++)for(int k=0;k<3;k++){in>>games[own][f][k]>>wins[own][f][k];if(!isfinite(wins[own][f][k])||!isfinite(games[own][f][k])||games[own][f][k]<0||games[own][f][k]>100000||wins[own][f][k]<0||wins[own][f][k]>games[own][f][k]){games[own][f][k]=wins[own][f][k]=0;}}for(auto& row:seen)for(int& n:row){in>>n;n=max(0,min(n,100000));}for(int& n:mistakes)in>>n;for(int& n:good)in>>n;
 // Optional tail keeps existing ADAPT_2 files readable.
 for(auto& t:tempo){double c=0,p=0;if(!(in>>c>>p))break;if(isfinite(c)&&isfinite(p)&&c>=0&&p>=0&&c+p<=100)t={c,p};}
 string marker;if(in>>marker&&marker=="STYLE_BELIEF_1")for(auto& b:behaviour){
  int n=0;array<double,3> e{};bool valid=bool(in>>n>>e[0]>>e[1]>>e[2]);
  for(double x:e)valid&=isfinite(x)&&x>=-6&&x<=0;
 if(valid&&n>=0&&n<=100000){b.evidence=e;b.observations=n;}
 }
 if(in>>marker&&marker=="ROUND_PLAN_BELIEF_1")for(auto& b:roundPlans){
  int n=0;array<double,3> e{};bool valid=bool(in>>n>>e[0]>>e[1]>>e[2]);
  for(double x:e)valid&=isfinite(x)&&x>=-6&&x<=0;
  if(valid&&n>=0&&n<=100000){b.evidence=e;b.observations=n;}
 }
 if(in>>marker&&marker=="CHARACTER_HISTORY_1"){
  CharacterHistory parsed[5][5];bool valid=true;
  for(auto& row:parsed)for(auto& h:row){
   valid&=bool(in>>h.last>>h.streak);
   valid&=(h.last==-1&&h.streak==0)||(h.last>=0&&h.last<3&&h.streak>=1&&h.streak<=2);
  }
  if(valid)for(int own=0;own<5;own++)for(int f=0;f<5;f++)characters[own][f]=parsed[own][f];
 }
 if(in>>marker&&marker=="CONTEXT_RESPONSE_1"){ResponseModel parsed[5];bool valid=true;for(auto& m:parsed)valid&=readResponse(in,m);if(valid)for(int f=0;f<5;f++)responses[f]=parsed[f];}
 if(in>>marker&&marker=="PREPARATION_HISTORY_1"){PreparationHistory parsed[5];bool valid=true;for(auto& h:parsed){valid&=bool(in>>h.observations)&&h.observations>=0&&h.observations<=100000;for(double& v:h.features){valid&=bool(in>>v)&&isfinite(v)&&v>=0&&v<=1000;}}if(valid)for(int f=0;f<5;f++)preparation[f]=parsed[f];}
 }
 void save(){if(path.empty())return;string temp=path+".tmp";ofstream out(temp);out.precision(17);out<<"GWENT_ADAPT_2\n";for(int own=0;own<5;own++)for(int f=0;f<5;f++)for(int k=0;k<3;k++)out<<games[own][f][k]<<' '<<wins[own][f][k]<<' ';out<<'\n';for(auto& row:seen)for(int n:row)out<<n<<' ';for(int n:mistakes)out<<n<<' ';for(int n:good)out<<n<<' ';for(auto t:tempo)out<<t.continued<<' '<<t.passed<<' ';out<<"STYLE_BELIEF_1 ";for(auto b:behaviour){out<<b.observations<<' ';for(double x:b.evidence)out<<x<<' ';}out<<"ROUND_PLAN_BELIEF_1 ";for(auto b:roundPlans){out<<b.observations<<' ';for(double x:b.evidence)out<<x<<' ';}out<<"CHARACTER_HISTORY_1 ";for(auto& row:characters)for(auto h:row)out<<h.last<<' '<<h.streak<<' ';out<<"CONTEXT_RESPONSE_1 ";for(auto m:responses)writeResponse(out,m);out<<"PREPARATION_HISTORY_1 ";for(auto h:preparation){out<<h.observations<<' ';for(double v:h.features)out<<v<<' ';}out.close();if(out)rename(temp.c_str(),path.c_str());}
 array<double,3> profileValues(int own,int faction)const{
  array<double,3> values{};double total=3;for(double n:games[own][faction])total+=n;
  for(int k=0;k<3;k++)values[k]=(wins[own][faction][k]+2)/(games[own][faction][k]+4)+.3*sqrt(log(total)/(games[own][faction][k]+1));
  return values;
 }
 int profile(int own,int faction,Rng& r)const{
  // Keep the established strength/exploration selector and its RNG stream.
  auto values=profileValues(own,faction);int result=0;
  if(r.u()<.25)result=r.n(3);
  else{double best=-1;for(int k=0;k<3;k++){double value=values[k]+.08*r.u();if(value>best){best=value;result=k;}}}
  auto h=characters[own][faction];
  if(h.streak<2||h.last!=result)return result;
  // Break a third repeat only among near-tied matchup priorities. A clearly
  // better character can repeat; diversity never overrides a searched move.
  double best=*max_element(values.begin(),values.end());vector<int> alternatives;
  for(int k=0;k<3;k++)if(k!=h.last&&values[k]>=best-.08)alternatives.push_back(k);
  return alternatives.empty()?result:alternatives[r.n(alternatives.size())];
 }
 void rememberProfile(int own,int faction,int style){
  auto& h=characters[own][faction];h.streak=h.last==style?min(2,h.streak+1):1;h.last=style;
 }
} memory;
int activeProfile=0;bool learnedResult=false;
Deck adaptDeck(Deck base,int opponent,int profile){
 // Keep complete unit packages and the 22-unit floor; vary the support plan.
 vector<int> keep;for(int c:base.cards)if(!C[c].kind||c==DEC||c==MARD)keep.push_back(c);base.cards=keep;
 auto put=[&](int c){int special=0;for(int x:base.cards)special+=!!C[x].kind;if(special<10&&count(base.cards.begin(),base.cards.end(),c)<C[c].limit)base.cards.push_back(c);};
 int weather=id(opponent==0?"Torrential Rain":opponent==2||opponent==1?"Biting Frost":"Impenetrable Fog");
 put(CLEAR);put(SCORCH);if(profile!=1)put(weather);if(profile==0){put(SCORCH);put(weather);}if(profile==1){put(HORN);put(SCORCH);}if(profile==2){put(CLEAR);put(HORN);}
 int weatherSeen=0,unitSeen=0;for(int c=0;c<(int)C.size();c++){weatherSeen+=C[c].weather?memory.seen[opponent][c]:0;unitSeen+=!C[c].kind?memory.seen[opponent][c]:0;}if(weatherSeen>=3&&weatherSeen*8>unitSeen)put(CLEAR);
 // Hero-heavy alternative reduces exposure to weather/scorch without removing spies/medics/bonds/muster.
 if(profile==2){vector<int> heroes;for(int c=0;c<(int)C.size();c++)if(C[c].collectible&&C[c].flags&HERO&&(C[c].faction<0||C[c].faction==base.faction)&&!count(base.cards.begin(),base.cards.end(),c))heroes.push_back(c);sort(heroes.begin(),heroes.end(),[](int a,int b){return C[a].p>C[b].p;});for(int hero:heroes){auto it=min_element(base.cards.begin(),base.cards.end(),[](int a,int b){auto weight=[](int c){return C[c].kind||C[c].flags?1000:C[c].p;};return weight(a)<weight(b);});if(it!=base.cards.end()&&!C[*it].kind&&!C[*it].flags&&C[hero].p>C[*it].p)*it=hero;}}
 base.name+="_plan"+to_string(profile);return base;
}
// Soft roster relationships, not an assumption that every player owns a full
// combo. A quarter of hypotheses retain the independent-card proposal.
double rosterCoherence(int card,const vector<int>& roster,int leader){
 auto x=C[card];int partners=0,transforms=0,activators=0;
 for(int c:roster){
  if((x.flags&(BOND|MUSTER|BERSERK)||card==id("Ermion"))&&packageKey(c)==packageKey(card))partners++;
  transforms+=!!(C[c].flags& BERSERK);activators+=c==MARD||c==id("Ermion");
 }
 double factor=1;
 if(partners)factor*=1+min(4,partners*2);
 if(card==MARD||card==id("Ermion"))factor*=transforms?(activators?.7:3.0):1.0;
 if(x.flags& BERSERK)factor*=activators?2.0:1.0;
 return min(6.0,factor);
}
#include "coherent_roster.inc"
bool coherentBeliefsEnabled=true;
vector<Deck> publicDeckHypotheses(const Game& obs,const Deck& own,int total,uint64_t seed,bool coherent=false){
 int side=obs.turn,other=1-side;map<int,int> required;for(auto&p:obs.p){for(int c:p.grave)required[c]++;for(auto u:p.board)required[u.id]++;}for(auto w:obs.weather)required[w.second]++;for(auto q:obs.openingQueue)required[q.second]++;for(int c:obs.p[side].hand)required[c]++;for(int c:obs.p[side].deck)required[c]++;for(int c:obs.p[other].known)required[c]++;for(int c:own.cards)required[c]--;for(auto [c,n]:obs.changes)required[c]-=n;
 vector<int> mandatory;for(auto[c,n]:required)for(int k=0;k<n;k++)mandatory.push_back(c);vector<Deck> result;Rng rng(seed);
 for(int sample=0;sample<12;sample++){if(coherentBeliefsEnabled&&sample>=4){auto structured=coherentPublicRoster(obs.p[other].faction,obs.p[side].faction,obs.p[other].leader,total,mandatory,(sample-4)%3,seed+sample*9179);if(structured){result.push_back(*structured);continue;}}Deck d;d.name="public_"+to_string(sample);d.faction=obs.p[other].faction;d.leader=obs.p[other].leader;d.cards=mandatory;int units=0,specials=0;for(int c:d.cards){units+=!C[c].kind;specials+=!!C[c].kind;}
  while((int)d.cards.size()<total){vector<int> candidates;vector<double> weights;double sum=0;for(int c=0;c<(int)C.size();c++){auto x=C[c];if(!x.collectible||(x.faction>=0&&x.faction!=d.faction)||count(d.cards.begin(),d.cards.end(),c)>=x.limit)continue;if(x.kind&&(specials>=10||total-(int)d.cards.size()<=22-units))continue;double w=.6+quality(c)/8.0;if(x.flags&(SPY|MED))w*=2;if(x.flags&(BOND|MUSTER))w*=1.5;if(c==DEC)w*=2;w*=1+min(1.0,memory.seen[d.faction][c]/20.0);if(coherent&&sample>=3)w*=rosterCoherence(c,mandatory,d.leader);candidates.push_back(c);weights.push_back(w);sum+=w;}if(candidates.empty())break;double roll=rng.u()*sum;int chosen=candidates.back();for(int i=0;i<(int)candidates.size();i++)if((roll-=weights[i])<=0){chosen=candidates[i];break;}d.cards.push_back(chosen);units+=!C[chosen].kind;specials+=!!C[chosen].kind;}
  if((int)d.cards.size()==total&&units>=22&&specials<=10)result.push_back(d);
 }return result;
}
array<double,3> opponentWeights(int faction){
 auto learned=memory.behaviour[faction].weights(),tempo=memory.tempo[faction].weights();
 for(int k=0;k<3;k++)learned[k]=.75*learned[k]+.25*tempo[k];
 return learned;
}
bool voluntaryRoundPlan(const Game& before,int viewer,Act played){
 int actor=1-viewer;
 return before.turn==actor&&before.round==0&&before.pending<0&&!before.opening&&
  !before.p[viewer].passed&&before.p[0].wins==0&&before.p[1].wins==0&&
  before.p[actor].hand.size()>=3&&(played.kind==0||played.kind==1||played.kind==3);
}
// Generalised observer path for both production (viewer 1) and isolated league
// seats. The one just-played card is now public; all other enemy cards/order
// are erased BEFORE constructing any hypothesis or likelihood.
void observeRoundPlanDecision(const Game& before,Act played,int viewer,const Deck& own,int enemyRosterSize,uint64_t seed){
 if(!voluntaryRoundPlan(before,viewer,played))return;
 auto obs=observation(before,viewer);obs.turn=viewer;int actor=1-viewer;
 if(played.kind==0&&find(obs.p[actor].known.begin(),obs.p[actor].known.end(),played.card)==obs.p[actor].known.end())obs.p[actor].known.push_back(played.card);
 if(obs.p[actor].known.size()>obs.p[actor].hand.size())return;
 auto lib=publicDeckHypotheses(obs,own,enemyRosterSize,seed);auto h=compatible(obs,viewer,own,lib);if(h.empty())return;
 int faction=before.p[actor].faction;
 memory.roundPlans[faction].observe(roundPlanLikelihood(obs,h,played,seed,opponentWeights(faction)));
}
void observeStyleDecision(const Game& before,Act a,int viewer,const Deck& own,int enemyRosterSize,uint64_t seed,bool detailedIntent=false,bool learnRoundPlans=false){
 int actor=1-viewer;
 if(viewer<0||viewer>1||own.cards.size()<22||enemyRosterSize<22||before.turn!=actor||before.pending>=0||before.opening||before.p[viewer].passed||before.p[actor].hand.size()<3||!(a.kind==0||a.kind==1||a.kind==3))return;
 // The played card becomes public evidence. Do not read any other human card.
 auto obs=observation(before,viewer);obs.turn=viewer;
 if(a.kind==0&&find(obs.p[actor].known.begin(),obs.p[actor].known.end(),a.card)==obs.p[actor].known.end())obs.p[actor].known.push_back(a.card);
 if(obs.p[actor].known.size()>obs.p[actor].hand.size())return;
 auto lib=publicDeckHypotheses(obs,own,enemyRosterSize,seed);
 auto h=compatible(obs,viewer,own,lib);if(h.empty())return;
 memory.behaviour[before.p[actor].faction].observe(styleLikelihood(obs,h,a,seed,detailedIntent));
 if(learnRoundPlans&&voluntaryRoundPlan(before,viewer,a))memory.roundPlans[before.p[actor].faction].observe(roundPlanLikelihood(obs,h,a,seed,opponentWeights(before.p[actor].faction)));
}
void observeOpponentDecision(const Game& before,Act a){
 auto seed=uint64_t(before.steps+1)*0x9e3779b97f4a7c15ULL;
 observeStyleDecision(before,a,1,decks[1],decks[0].cards.size(),seed,false,true);
}
void observeResponseDecision(const Game& before,const Game& after,Act played,int viewer,const Deck& own,int enemySize,uint64_t seed){
 int actor=1-viewer;if(viewer<0||viewer>1||before.turn!=actor||own.cards.size()<22||enemySize<22)return;
 int f=before.p[actor].faction;auto& model=memory.responses[f];
 observeResponseCounter(before,after,played,actor,model.habits[3],memory.responseWindows[f]);
 if(!responseVoluntary(before,actor))return;
 auto obs=observation(before,viewer);obs.turn=viewer;
 if(played.kind==0&&find(obs.p[actor].known.begin(),obs.p[actor].known.end(),played.card)==obs.p[actor].known.end())obs.p[actor].known.push_back(played.card);
 if(obs.p[actor].known.size()>obs.p[actor].hand.size())return;
 auto lib=publicDeckHypotheses(obs,own,enemySize,seed);auto h=compatible(obs,viewer,own,lib);if(h.empty())return;
 if(before.round==0&&!eliminationPressure(before))model.habits[0].observe(responseLikelihood(obs,h,played,0,seed));
 if(before.round==1&&before.p[actor].wins==1&&before.p[viewer].wins==0)model.habits[1].observe(responseLikelihood(obs,h,played,1,seed));
 // Only an agile card publicly proves that BOTH rows were available.
 if(played.kind==0&&C[played.card].row==3&&!(C[played.card].flags&SPY)&&responseLoadedRow(before,actor)>=0)model.habits[2].observe(responseLikelihood(obs,h,played,2,seed));

}
ResponseModel opponentResponse(int faction){auto model=memory.responses[faction];model.enabled=true;return model;}

void learnPublicMove(const Game& before,const Game& after,Act a){int side=before.turn;observeOpponentDecision(before,a);if(side==0)observeResponseDecision(before,after,a,1,decks[1],decks[0].cards.size(),uint64_t(before.steps+1)*0xa71932ULL);if(side==0&&(a.kind==0||a.kind==2)&&a.card>=0){memory.seen[before.p[0].faction][a.card]=min(100000,memory.seen[before.p[0].faction][a.card]+1);memory.preparation[before.p[0].faction].observe(a.card);}
 // Observe only comparable, voluntary first-round decisions: no forced play,
 // pending choices, empty hands, already-passed opponents or match-point rounds.
 if(side==0&&before.round==0&&before.pending<0&&!before.p[1].passed&&before.p[0].hand.size()>=3){
  int gap=score(before,1)-score(before,0);
  if(gap>=2&&gap<=16&&(a.kind==0||a.kind==1||a.kind==3))memory.tempo[before.p[0].faction].observe(a.kind==3);
 }
 // Only label a tactical error where an exhaustive public catch-up proves it.
 if(side==1&&before.pending<0&&before.p[1-side].passed&&before.p[1-side].wins==1){auto plan=catchUp(observation(before,side),1000);if(plan.complete&&plan.winning){bool failed=after.winner>=0&&after.winner!=side;if(after.round==before.round&&after.winner<0&&after.pending<0&&!ahead(after,side)){auto next=catchUp(observation(after,side),1000);failed=next.complete&&!next.winning;}if(failed)memory.mistakes[side]++;else if(after.winner==side||ahead(after,side))memory.good[side]++;}}
 if(after.winner>=0&&!learnedResult){int f=after.p[0].faction;memory.games[after.p[1].faction][f][activeProfile]++;memory.wins[after.p[1].faction][f][activeProfile]+=after.winner==1?1:after.winner==2?.5:0;learnedResult=true;memory.save();}
}

#include "history_preparation.inc"
// Matchup-aware prior over leaders. Uses only faction, public history and own planned units.
Deck preparedDeck(int faction,int opponent,int style,int leader){
 // Package builds qualified for Monsters/Skellige; retain the established
 // roster in factions where the candidate did not improve the benchmark.
 auto deck=faction==2||faction==4?buildStrategyDeck(faction,opponent,style,leader):adaptDeck(preset(faction,leader),opponent,style);return historyPreparedDeck(deck,opponent);
}
int chooseLeader(int faction,int opponent,int /*hiddenHumanLeader*/,uint64_t seed,int style=0){
 Rng r(seed^0x1eaDe2);int best=factionLeaders[faction][0];double high=-1e9;
 for(int l:factionLeaders[faction]){auto d=preparedDeck(faction,opponent,style,l);double value=0;int rows[3]={},med=0,spy=0;for(int c:d.cards){if(C[c].kind)continue;if(C[c].row<3&&(!historyPreparationEnabled||!(C[c].flags&HERO)))rows[C[c].row]+=C[c].p;med+=!!(C[c].flags&MED);spy+=!!(C[c].flags&SPY);}
 switch(l){case 0:value=rows[2]*.65;break;case 1:value=opponent==0?19:12;break;case 2:value=opponent==3?19:8;break;case 6:value=opponent==3?10:3;break;case 7:value=opponent==0?21:6;break;case 3:value=18+(opponent<2?9:0);break;case 4:value=15;break;case 5:value=opponent==0?17:3;break;case 8:value=7;break;case 9:value=opponent==0?6:2;break;case 10:value=rows[0]*.45;break;case 11:value=12;break;case 12:value=15;break;case 13:value=19;break;case 14:value=opponent<2?-12:3;break;case 15:value=15;break;case 16:value=rows[1]*.6;break;case 17:value=opponent==2?23:11;break;case 18:value=opponent==2?18:8;break;case 19:value=8;break;case 20:value=opponent<2?24:12;break;case 21:value=18;break;}
 value+=historyLeaderBonus(l,opponent,d);value+=r.u()*5;if(value>high){high=value;best=l;}}
 return best;
}
