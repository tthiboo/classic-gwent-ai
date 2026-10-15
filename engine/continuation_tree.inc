// Search future OWN decisions by what that player can observe. Unlike an
// action-history tree, transpositions after different play orders share work.
// Enemy responses remain sampled from the public behaviour model. No concealed
// enemy hand or ordered draw pile is present in a node key.
#include "continuation_transfer.inc"
struct ContinuationTree {
 unordered_map<string,NodeV9> nodes;
 unordered_map<string,ContinuationTransferRecord> transferRecords;bool captureTransfer=false;
 using Path=vector<pair<NodeV9*,int>>;
 int depth=1,concreteHintNodes=0,pairHintNodes=0;bool roundResources=false,concreteLessons=false,learnedPairPriors=false,matchPlans=false;
 MatchPlanTree plans;const SearchActor* actor=nullptr;int actorNodes=0;bool actorPriorsOnly=false;
 Act choose(const Game& g,Rng& rng,int style,Path& path,int level,int plan=0) {
  // Medic chains and tutor choices already have a dedicated complete-chain
  // planner. An under-visited general tree must not replace it with flat priors
  // for every unranked resurrection target.
  if(g.pending>=0||g.opening)return fastV8(g,rng,style,64);
  auto catchPlan=catchUp(g,64);if(catchPlan.complete)return catchPlan.first;
  string key=informationKeyV9(g,g.turn,roundResources);if(matchPlans)key+="/plan:"+to_string(plan);
  // Capacity limits NEW observations only. Keeping a full tree must not
  // disable all already explored continuations or their outcome updates.
  if(nodes.size()>=4096&&nodes.find(key)==nodes.end())return actor&&!actorPriorsOnly?actorFuturePolicy(g,rng,style,plan,actor):fastV8(g,rng,style,64);
  auto& node=nodes[key];
  if(node.edges.empty()) {
   auto safe=safeActionsV9(g);vector<Scored> ranked;vector<double> learned;
   if(actor){learned=actorPlanProbabilities(g,safe,rng,style,matchPlans?plan:0,*actor);actorNodes++;}
   else ranked=matchPlans?rankMatchPlan(g,rng,style,plan):rankMicro(g,rng,style);double total=0;
   auto hints=concreteLessons?moveLearning.adjustments(g,safe):vector<double>(safe.size());
   int choice=0;for(auto a:safe){int rank=0;while(rank<(int)ranked.size()&&!(ranked[rank].a==a))rank++;
    double prior=actor?learned[choice++]:(.12/safe.size()+.88*exp(-.55*rank));total+=prior;node.edges.push_back({a,prior});}
   if(concreteLessons){applyMoveHints(node.edges,hints);concreteHintNodes+=any_of(hints.begin(),hints.end(),[](double x){return x!=0;});}
   if(learnedPairPriors){pairHintNodes+=applyPairPriors(node.edges,observation(g,g.turn));total=0;for(auto& e:node.edges)total+=e.prior;}
   for(auto& e:node.edges)e.prior/=total;
   if(captureTransfer){auto& record=transferRecords[key];record.context=continuationTransferContext(g,plan);
    for(auto& e:node.edges)record.bindings.push_back(continuationBinding(g,e.action));}
  }
  double sum=0;for(auto& e:node.edges)sum+=e.value;
  double parent=(sum+.5)/(node.visits+1),best=-1e100;int selected=0;
  for(int i=0;i<(int)node.edges.size();i++){
   auto& e=node.edges[i];double q=(e.value+2*parent)/(e.visits+2);
   double u=q+1.2*e.prior*sqrt(node.visits+1.0)/(e.visits+1);
   if(u>best){best=u;selected=i;}
  }
  path.push_back({&node,selected});depth=max(depth,level);
  return node.edges[selected].action;
 }
 void learn(const Path& path,double outcome){for(auto [node,index]:path){node->visits++;node->edges[index].visits++;node->edges[index].value+=outcome;}}
 Act replayPolicy(const Game& g,Rng& rng,int style,int plan=0)const{
  auto key=informationKeyV9(g,g.turn,roundResources);if(matchPlans)key+="/plan:"+to_string(plan);
  auto found=nodes.find(key);
  if(found==nodes.end()||found->second.visits<8)return matchPlans?matchPlanPolicy(g,rng,style,plan):fastV8(g,rng,style,64);
  const auto& edges=found->second.edges;
  auto best=max_element(edges.begin(),edges.end(),[](const auto& a,const auto& b){return a.visits!=b.visits?a.visits<b.visits:a.prior<b.prior;});
  if(best->visits<4)return matchPlans?matchPlanPolicy(g,rng,style,plan):fastV8(g,rng,style,64);
  return best->action;
 }
};
struct ContinuationPortfolio {vector<Act> roots;vector<ContinuationTree> trees;array<double,3> enemyPlans={1,0,0};ResponseModel response;};
