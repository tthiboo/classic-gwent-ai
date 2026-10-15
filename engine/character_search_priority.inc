// Preserve the mature continuation policies and empirical return comparison.
// Character allocates the remaining search among tactical spending proposals that
// agree on EVERY common sampled match, not merely on aggregate win counts.
// Permuting the group's prior slots keeps its precedence against other roles
// and gives the sorting comparator a globally consistent numerical order.
struct CharacterPlanTrace {
 int side,round,actor=0,hand=0;bool finished=false,counter=false;
 double ownBefore=0,enemyBefore=0;array<double,3> value{};
 CharacterPlanTrace(const Game& g,int s):side(s),round(g.round){}
 void before(const Game& g,Act a){
  if(finished)return;actor=g.turn;hand=g.p[actor].hand.size();
  counter=actor==side&&a.kind==0&&(decisionClass(a)==5||decisionClass(a)==6);
  if(counter){ownBefore=score(g,side);enemyBefore=score(g,1-side);}
 }
 void after(const Game& g){
  if(finished)return;int spent=hand-int(g.p[actor].hand.size());value[1]+=actor==side?-spent:spent;
  if(counter&&g.round==round&&g.winner<0)value[0]+=max(0.0,enemyBefore-score(g,1-side)+min(0.0,score(g,side)-ownBefore))/10;
  if(g.round!=round||g.winner>=0){
   for(int c:g.p[side].hand)value[2]+=(max(quality(c),(C[c].flags&UHORN)?quality(HORN):0.0)+(C[c].flags&MED?10:0))/10;
   finished=true;
  }
 }
};
vector<double> characterSearchPriorities(const Game& obs,ReportV9& report,
 const vector<int>& active,const vector<string>& histories,int style,
 const vector<array<double,3>>* returns=nullptr){
 vector<double> result;for(auto e:report.root)result.push_back(e.prior);
 if(style<0||style>2||obs.pending>=0||obs.opening||obs.p[1-obs.turn].passed)return result;
 map<pair<int,string>,vector<int>> groups;
 for(int i:active){auto e=report.root[i];int role=decisionClass(e.action);
  if(e.action.kind!=0||role==3||role==4||histories[i].size()<8||e.value<.5*e.visits)continue;
  groups[{0,histories[i]}].push_back(i);
 }
 for(auto& entry:groups){auto indices=entry.second;if(indices.size()<2)continue;
  vector<pair<CharacterPreference,int>> choices;vector<double> slots;
  for(int i:indices){auto preference=characterPreference(obs,report.root[i].action,style);if(preference){choices.push_back({*preference,i});slots.push_back(report.root[i].prior);}}
  if(choices.size()<2)continue;
  sort(slots.begin(),slots.end(),greater<double>());
  sort(choices.begin(),choices.end(),[&](auto a,auto b){
   if(returns&&(*returns)[a.second][style]!=(*returns)[b.second][style])return (*returns)[a.second][style]>(*returns)[b.second][style];
   return a.first!=b.first?a.first>b.first:report.root[a.second].prior>report.root[b.second].prior;
  });
  bool changed=false;for(int k=0;k<int(choices.size());k++){
   int i=choices[k].second;result[i]=slots[k];changed|=result[i]!=report.root[i].prior;
  }
  if(changed){report.characterPriorityGroups++;report.characterPriorityReorders+=choices.size();}
 }
 return result;
}
