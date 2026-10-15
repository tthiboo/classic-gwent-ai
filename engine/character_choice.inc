#include <optional>
// Character is a secondary objective AFTER completed search. Never change
// rollout allocation, empirical outcomes, irreversible passes, leader use or
// public proofs. Exact equal evidence is not proof of equal true strength.
using CharacterPreference=array<double,5>;
double characterInventoryValue(const Game& g,int side){
 double value=0;for(auto* pile:{&g.p[side].hand,&g.p[side].deck})for(int c:*pile)
  value+=max(quality(c),(C[c].flags&UHORN)?quality(HORN):0.0);
 return value;
}
optional<CharacterPreference> characterPreference(const Game& actual,Act a,int style){
 if(style<0||style>2||a.kind!=0)return nullopt;
 auto g=observation(actual,actual.turn);int side=g.turn,other=1-side;
 Rng preview(0x542ac173ULL);auto after=previewTurn(g,a,preview,20);
 if(after.round!=g.round||after.winner>=0)return nullopt;
 double ownGain=score(after,side)-score(g,side),enemyLoss=score(g,other)-score(after,other);
 double handGain=int(after.p[side].hand.size())-int(g.p[side].hand.size());
 // Hand + deck multiset cancels drawn identities; only actually committed or
 // retrieved known resources count. Summon packages and unit horns are valued.
 double cost=characterInventoryValue(g,side)-characterInventoryValue(after,side);
 double risk=globalExposure(after,side)-globalExposure(g,side);
 if(style==1)return CharacterPreference{ownGain+enemyLoss,handGain,-cost,0,0};
 if(style==2)return CharacterPreference{handGain,-cost,ownGain+enemyLoss,0,0};
 // A "counter" that removes more own points than enemy points is not useful
 // disruption. Do not confuse lowering our score with protecting it from Scorch.
 return CharacterPreference{max(0.0,enemyLoss+min(0.0,ownGain)),handGain,ownGain+enemyLoss,-risk,-cost};
}
void chooseCharacterTie(const Game& actual,ReportV9& report,int style){
 if(actual.pending>=0||actual.opening||actual.p[1-actual.turn].passed||report.chosen.kind!=0)return;
 auto chosen=find_if(report.root.begin(),report.root.end(),[&](auto e){return e.action==report.chosen;});
 if(chosen==report.root.end()||chosen->visits<8||chosen->value<.5*chosen->visits)return;
 auto safe=safeActionsV9(observation(actual,actual.turn));vector<Act> peers;
 for(auto e:report.root)if(!(e.action==report.chosen)&&e.action.kind==0&&e.visits==chosen->visits&&e.value==chosen->value&&find(safe.begin(),safe.end(),e.action)!=safe.end())peers.push_back(e.action);
 if(peers.empty())return;auto preference=characterPreference(actual,report.chosen,style);if(!preference)return;
 report.characterTieOriginal=report.chosen;
 for(auto a:peers){auto other=characterPreference(actual,a,style);if(!other)continue;report.characterTieCandidates++;
  if(*other>*preference){report.chosen=a;preference=other;}
 }
 report.characterTieChanged=!(report.chosen==report.characterTieOriginal);
}
