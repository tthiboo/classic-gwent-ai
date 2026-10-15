#include "bot_v8.cpp"
#include <chrono>
#include "decision_learning.inc"
#include "value_model.inc"

// Finite-budget observer information-set tree with a sampled opponent policy. Values are simulated outcomes, not
// calibrated win probabilities. Hidden-world sampling is still approximate.
struct BudgetV9 { int iterations=256, milliseconds=250, depth=18; bool coverRoot=true; };
struct EdgeV9 { Act action; double prior=0,value=0; int visits=0; };
struct NodeV9 { vector<EdgeV9> edges; int visits=0; };
struct ContinuationPortfolio;
struct ReportV9 {
 Act chosen; int iterations=0,nodes=0,depth=0,reviewTrials=0; double milliseconds=0,reviewMilliseconds=0;
 int verifiedLessonMatches=0;bool verifiedLessonProof=false;int passTrials=0;bool passOverridden=false;double passMargin=0,passMilliseconds=0;
 int concreteReviewTrials=0,concreteHintNodes=0;bool concreteLesson=false,concreteCorrected=false;
 int pairHintNodes=0,matchPlanNodes=0;array<int,3> matchPlanTrials{};
 int trajectoryLeaves=0,actorNodes=0;
 int characterPriorityGroups=0,characterPriorityReorders=0;bool responseForecastEnabled=false;int responseActiveHabits=0;
 int characterTieCandidates=0;bool characterTieChanged=false;Act characterTieOriginal;double characterTieMilliseconds=0;
 double concreteMargin=0,concreteStandardError=0;
 Act concreteOriginal,concreteAlternative;double concretePrediction=0;
 vector<pair<double,double>> concreteOutcomes;
 vector<EdgeV9> root;
 shared_ptr<ContinuationPortfolio> continuations;
};
string informationKeyV9(const Game& g,int actor,bool roundResources=false) {
 ostringstream out;out<<publicKey(g)<<'#'<<privateKey(g,actor)<<'#'
 <<g.pendingKind<<','<<g.remaining<<','<<g.opening<<','<<g.openingTurn<<';';
 if(roundResources)for(auto& p:g.p)out<<p.faction<<','<<p.drawn<<';';
 for(auto q:g.openingQueue)out<<q.first<<':'<<q.second<<',';
 return out.str();
}
#include "move_learning.inc"
vector<Act> safeActionsV9(const Game& g);
#include "verified_lessons.inc"
#include "pair_value.inc"
#include "trajectory_value.inc"
#include "search_actor.inc"
#include "opening_discipline.inc"
vector<Act> safeActionsV9(const Game& g) {
 auto actions=legal(g);
 actions.erase(remove_if(actions.begin(),actions.end(),[&](Act a){
  return emptyCrach(g,a)||emptyScorchLeader(g,a)||selfOnlyScorch(g,a);
 }),actions.end());
 if(eliminationPressure(g)&&any_of(actions.begin(),actions.end(),[](Act a){return a.kind!=3;}))
  actions.erase(remove_if(actions.begin(),actions.end(),[](Act a){return a.kind==3;}),actions.end());
 openingDiscipline(g,actions);
 return actions;
}
#include "character_choice.inc"
ReportV9 searchV9(const Game& actual,const vector<Hypothesis>& hypotheses,uint64_t seed,
 int style=0,array<double,3> enemyWeights={1,1,1},BudgetV9 budget={}) {
 if(budget.iterations<1||budget.depth<1)throw runtime_error("invalid v9 budget");
 auto began=chrono::steady_clock::now();
 auto elapsed=[&](){return chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();};
 Game observed=observation(actual,actual.turn);int viewer=observed.turn;Rng rng(seed);
 unordered_map<string,NodeV9> tree;ReportV9 report;
 for(int iteration=0;iteration<budget.iterations;iteration++) {
  // Stop between complete simulations: one simulation may overrun the soft limit.
  if(iteration&&budget.milliseconds>0&&elapsed()>=budget.milliseconds)break;
  Game g=sampleBelief(observed,viewer,hypotheses,rng);g.rng=Rng(rng.next());
  int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
  array<string,2> memory={privateKey(g,0),privateKey(g,1)};string history;
  vector<pair<string,int>> path;
  for(int depth=0;g.winner<0&&depth<budget.depth;depth++) {
   int actor=g.turn;
   // Searching a separate node for every sampled enemy hand fragments the tree
   // into almost-unvisited branches. Model enemy turns using the adaptive policy
   // mixture; share OUR decisions across worlds with the same observations.
   if(actor!=viewer){
    auto a=fastV8(g,rng,enemyStyle,64);apply(g,a);
    history+=';'+to_string(actor)+':'+actionKey(a);
    for(int s=0;s<2;s++)memory[s]+='|'+privateKey(g,s);
    continue;
   }
   string key=depth==0?"ROOT":history+'#'+informationKeyV9(g,actor)+'#'+memory[actor];
   auto& node=tree[key];
   if(node.edges.empty()) {
    auto actions=safeActionsV9(g);
    // Priors use actor-visible evidence through the existing blind ranking.
    auto ranked=rankMicro(g,rng,actor==viewer?style:enemyStyle);
    double total=0;
    for(auto a:actions){int rank=0;for(;rank<(int)ranked.size();rank++)if(ranked[rank].a==a)break;
     double prior=.12/actions.size()+.88*exp(-.55*rank);if(depth==0)prior*=decisionLearning.multiplier(learningContext(observed),decisionClass(a));total+=prior;node.edges.push_back({a,prior});}
    for(auto& e:node.edges)e.prior/=total;
   }
   int selected=0;double best=-1e100;
   for(int j=0;j<(int)node.edges.size();j++) {
    auto& e=node.edges[j];double q=e.visits?e.value/e.visits:.5;
    if(actor!=viewer)q=1-q;
    double u=q+1.4*e.prior*sqrt(node.visits+1.0)/(e.visits+1);
    if(u>best){best=u;selected=j;}
   }
   // Give every safe root move one complete trial before concentrating visits.
   // Rank orders this initial sweep, but cannot starve a low-prior tactic.
   if(depth==0&&budget.coverRoot){int untried=-1;
    for(int j=0;j<(int)node.edges.size();j++)if(!node.edges[j].visits&&(untried<0||node.edges[j].prior>node.edges[untried].prior))untried=j;
    if(untried>=0)selected=untried;
   }
   auto a=node.edges[selected].action;bool fresh=node.edges[selected].visits==0;
   path.push_back({key,selected});apply(g,a);
   history+=';'+to_string(actor)+':'+actionKey(a);
   for(int s=0;s<2;s++)memory[s]+='|'+privateKey(g,s);
   report.depth=max(report.depth,depth+1);
   if(fresh)break;
  }
  // Independent styles, with a nonzero generalist mixture supplied by memory.
  while(g.winner<0) {
   if(g.steps>500)throw runtime_error("long v9 rollout");
   apply(g,fastV8(g,rng,g.turn==viewer?style:enemyStyle,64));
  }
  double outcome=g.winner==2?.5:g.winner==viewer?1:0;
  for(auto& step:path){auto& n=tree.at(step.first);n.visits++;auto& e=n.edges[step.second];e.visits++;e.value+=outcome;}
  report.iterations++;
 }
 report.root=tree.at("ROOT").edges;report.nodes=tree.size();
 auto best=max_element(report.root.begin(),report.root.end(),[](auto& a,auto& b){return a.visits<b.visits;});
 report.chosen=best->action;report.milliseconds=elapsed();return report;
}
// Independent paired challenge of an irreversible pass. The alternative is
// fixed before fresh worlds are sampled, avoiding selection on validation data.
void challengePass(const Game& obs,const vector<Hypothesis>& hypotheses,uint64_t seed,
 int style,array<double,3> enemyWeights,ReportV9& report,int maxTrials=24,int milliseconds=35){
 if(report.chosen.kind!=3||obs.pending>=0||obs.p[1-obs.turn].passed)return;
 auto safe=safeActionsV9(obs);const EdgeV9* alternative=nullptr;
 for(const auto& e:report.root)if(e.action.kind!=3&&find(safe.begin(),safe.end(),e.action)!=safe.end())
  if(!alternative||e.visits>alternative->visits||(e.visits==alternative->visits&&e.prior>alternative->prior))alternative=&e;
 if(!alternative)return;
 auto began=chrono::steady_clock::now();Rng rng(seed^0xadaef0137b25691ULL);
 int side=obs.turn;double sum=0,squares=0;
 for(int trial=0;trial<maxTrials;trial++){
  if(trial&&milliseconds>0&&chrono::duration<double,milli>(chrono::steady_clock::now()-began).count()>=milliseconds)break;
  auto world=sampleBelief(obs,side,hypotheses,rng);auto common=rng.next();world.rng=Rng(common);
  int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
  auto outcome=[&](Act a){Game g=world;Rng rollout(common);apply(g,a);
   while(g.winner<0){if(g.steps>500)throw runtime_error("long pass challenge");apply(g,fastV8(g,rollout,g.turn==side?style:enemyStyle,64));}
   return g.winner==2?.5:g.winner==side?1.0:0.0;
  };
  double difference=outcome(alternative->action)-outcome(report.chosen);sum+=difference;squares+=difference*difference;report.passTrials++;
 }
 if(report.passTrials){
  double n=report.passTrials,mean=sum/n,variance=max(0.0,(squares-sum*sum/n)/max(1.0,n-1));report.passMargin=mean;
  // Conservative practical margin, not a calibrated statistical guarantee.
  if(n>=12&&mean>.15+1.5*sqrt(variance/n)){report.chosen=alternative->action;report.passOverridden=true;}
 }
 report.passMilliseconds=chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();
}
// Review the searched move against the unlearned tactical baseline on fresh,
// shared worlds/seeds. No match outcome is assigned to every earlier action.
int reviewDecision(const Game& obs,const vector<Hypothesis>& h,Act chosen,Act baseline,uint64_t seed,int style,array<double,3> enemyWeights){
 if(obs.pending>=0||chosen==baseline||decisionClass(chosen)==decisionClass(baseline))return 0;
 auto safe=safeActionsV9(obs);if(find(safe.begin(),safe.end(),baseline)==safe.end())return 0;
 Rng rng(seed^0xc03f51d94a725b61ULL);int side=obs.turn,context=learningContext(obs),completed=0;
 auto began=chrono::steady_clock::now();
 for(int trial=0;trial<4;trial++){
  if(trial&&chrono::duration<double,milli>(chrono::steady_clock::now()-began).count()>=40)break;
  auto world=sampleBelief(obs,side,h,rng);auto common=rng.next();world.rng=Rng(common);
  int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
  auto outcome=[&](Act a){Game g=world;Rng r(common);apply(g,a);
   while(g.winner<0){if(g.steps>500)throw runtime_error("long learning rollout");apply(g,fastV8(g,r,g.turn==side?style:enemyStyle,64));}
   return g.winner==2?.5:g.winner==side?1.0:0.0;
  };
  double alternative=outcome(chosen),original=outcome(baseline);
  decisionLearning.observe(context,decisionClass(chosen),decisionClass(baseline),alternative-original);completed++;
 }
 return completed;
}
#include "root_race.inc"
#include "parallel_root.inc"
ReportV9 lastSearchV9;
ReportV7 playV9(const Game& actual,const Deck& own,const vector<Deck>& lib,uint64_t seed,
 bool search=true,int style=0,array<double,3> enemyWeights={1,1,1},bool validatePass=false,bool coverRoot=true,bool useRace=false) {
 lastSearchV9={};
 // Preserve deterministic catch-up and the mature exact/robust endgame path.
 // No extra full rollout search is run in this preliminary pass.
 auto fallback=playV8(actual,own,lib,seed,false,style,enemyWeights);
 if(!search||fallback.exact||fallback.guaranteed||fallback.catchComplete)return fallback;
 auto obs=observation(actual,actual.turn);auto h=compatible(obs,obs.turn,own,lib);
 // More paired continuations when they fit; retain the same soft time limit.
 lastSearchV9=useRace?raceRoot(obs,h,seed,style,enemyWeights,{512,250,18}):searchV9(obs,h,seed,style,enemyWeights,{256,250,18,coverRoot});
 if(validatePass)challengePass(obs,h,seed,style,enemyWeights,lastSearchV9);
 auto reviewStart=chrono::steady_clock::now();
 lastSearchV9.reviewTrials=reviewDecision(obs,h,lastSearchV9.chosen,fallback.chosen,seed,style,enemyWeights);
 lastSearchV9.reviewMilliseconds=chrono::duration<double,milli>(chrono::steady_clock::now()-reviewStart).count();
 fallback.chosen=lastSearchV9.chosen;return fallback;
}

// Keep the old entry point available for paired regression comparisons.
ReportV7 playV10(const Game& actual,const Deck& own,const vector<Deck>& lib,uint64_t seed,
 bool search=true,int style=0,array<double,3> enemyWeights={1,1,1}){
 return playV9(actual,own,lib,seed,search,style,enemyWeights,false,true,true);
}
#include "bot_v11.inc"
