// Successive halving on shared hidden worlds. Each complete batch compares
// every surviving move with identical deal, rollout seed and opponent style.
// No actual concealed opponent identities or own draw order are consulted.
// Alternative opponent plans for isolated qualification: resource preservation
// and deliberate first-round concession, rather than only different card values.
Act plannedOpponentReply(const Game& g,Rng& rng,int style,int plan){
 if(!plan||g.pending>=0||g.round>0)return fastV8(g,rng,style,64);
 int features=15|PASS_REPLY;
 if(plan==1)features|=4<<8;
 else features|=EARLY_PASS;
 auto action=fastV7(g,rng,style,features,64);auto safe=safeActionsV9(g);
 if(find(safe.begin(),safe.end(),action)!=safe.end())return action;
 return fastV8(g,rng,style,64);
}
// Plans use an independent stream: a neutral mixture reproduces v11 exactly.
int sampleRoundPlan(uint64_t common,const array<double,3>& weights){
 double total=0;for(double w:weights){if(!isfinite(w)||w<0)throw runtime_error("invalid round-plan mixture");total+=w;}
 if(total<=0)throw runtime_error("empty round-plan mixture");
 if(weights[1]==0&&weights[2]==0)return 0;
 Rng rng(common^0x629cab1578ULL);double roll=rng.u()*total;
 for(int i=0;i<3;i++)if((roll-=weights[i])<=0)return i;
 return 2;
}
// Distinct round budgets, without replacing the mature tactical policy.
// Preserve draw/decoy exchanges and every forced or already-closed round.
Act strategicOpponentPlan(const Game& g,Rng& rng,int style,int plan,Act action){
 int side=g.turn,other=1-side;
 if(!plan||g.pending>=0||g.opening||g.round>0||g.p[other].passed||g.p[0].wins||g.p[1].wins||g.p[other].hand.empty()||action.kind==3)return action;
 // Absolute hand thresholds fail after spies: a twelve-card hand may already
 // have spent several cards. Draw count minus remaining hand measures net
 // expenditure, including opening bonuses and card-preserving exchanges.
 int spent=max(0,g.p[side].drawn-(int)g.p[side].hand.size()),allowance=plan==1?4:2;
 if(spent<allowance)return action;
 auto after=previewTurn(g,action,rng,style==0?20:style==1?15:25);
 bool draws=after.p[side].hand.size()>g.p[side].hand.size();
 bool exchange=action.kind==0&&action.card==DEC&&after.p[side].hand.size()>=g.p[side].hand.size();
 return draws||exchange?action:Act{3};
}
Act strategicOpponentReply(const Game& g,Rng& rng,int style,int plan){
 auto action=fastV8(g,rng,style,64,true);return strategicOpponentPlan(g,rng,style,plan,action);
}
#include "match_plan.inc"
#include "opponent_response.inc"
// Learned actor is explicit research input. Apply the existing round plan to
// tactical scores BEFORE rank-to-prior conversion, just as the legacy tree.
// Adding point offsets to log rank probabilities changes even a zero actor.
vector<double> actorPlanProbabilities(const Game& g,const vector<Act>& moves,Rng& rng,int style,int plan,const SearchActor& actor){
 auto base=rankMatchPlan(g,rng,style,plan);vector<double> tactical;double sum=0;
 for(auto a:moves){int rank=0;while(rank<int(base.size())&&!(base[rank].a==a))rank++;
  tactical.push_back(.12/moves.size()+.88*exp(-.55*rank));sum+=tactical.back();}
 for(double& v:tactical)v/=sum;
 return actor.probabilities(g,moves,style,tactical);
}
Act actorFuturePolicy(const Game& g,Rng& rng,int style,int plan,const SearchActor* actor){
 if(!actor||!actor->hasResidual()||g.pending>=0||g.opening)return matchPlanPolicy(g,rng,style,plan);
 auto proof=catchUp(g,64);if(proof.complete)return proof.first;
 auto moves=safeActionsV9(g);auto p=actorPlanProbabilities(g,moves,rng,style,plan,*actor);
 return moves[max_element(p.begin(),p.end())-p.begin()];
}
#include "continuation_tree.inc"
ReportV9 raceRoot(const Game& actual,const vector<Hypothesis>& hypotheses,uint64_t seed,
 int style=0,array<double,3> enemyWeights={1,1,1},BudgetV9 budget={},bool variedPlans=false,bool learnedValue=false,bool valueTiesOnly=true,bool searchContinuations=false){
 if(budget.iterations<1)throw runtime_error("invalid root race budget");
 auto began=chrono::steady_clock::now();auto elapsed=[&](){return chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();};
 auto obs=observation(actual,actual.turn);int side=obs.turn;Rng rng(seed);ReportV9 report;
 auto safe=safeActionsV9(obs);auto priorWorld=sampleBelief(obs,side,hypotheses,rng);auto ranked=rankMicro(priorWorld,rng,style);double total=0;
 for(auto a:safe){int rank=0;while(rank<(int)ranked.size()&&!(ranked[rank].a==a))rank++;
  double prior=(.12/safe.size()+.88*exp(-.55*rank))*decisionLearning.multiplier(learningContext(obs),decisionClass(a));
  total+=prior;report.root.push_back({a,prior});
 }
 for(auto& e:report.root)e.prior/=total;
 vector<double> valueHints(report.root.size());
 if(learnedValue&&positionValue.enabled){
  // Independent stream: enabling the model cannot change root rollout worlds.
  // Average prospective draws/round transitions, rather than reading an actual
  // draw pile or using one accidental sampled world's private outcomes.
  Rng predictions(seed^0xc4ac09b553ULL);
  for(int sample=0;sample<8;sample++){
   auto world=sampleBelief(obs,side,hypotheses,predictions);world.rng=Rng(predictions.next());
   for(int i=0;i<(int)report.root.size();i++){auto g=world;apply(g,report.root[i].action);valueHints[i]+=positionValue.relative(valueFeatures(g,side))/8;}
  }
  double mean=accumulate(valueHints.begin(),valueHints.end(),0.0)/valueHints.size();
  // Relative, bounded hint (at most a quarter of one virtual observation).
  // Complete simulations, legality, exact proofs and safety remain authoritative.
  for(double& value:valueHints)value=clamp(value-mean,-.25,.25);
 }
 vector<int> active(report.root.size());iota(active.begin(),active.end(),0);
 auto order=[&](int a,int b){auto& x=report.root[a];auto& y=report.root[b];double qx=(x.value+.5+(valueTiesOnly?0:valueHints[a]))/(x.visits+1),qy=(y.value+.5+(valueTiesOnly?0:valueHints[b]))/(y.visits+1);
  if(qx!=qy)return qx>qy;
  // Before any simulation, retain the mature tactical ordering. The fitted
  // value may resolve measured ties, never reverse unequal rollout evidence.
  double hx=x.visits?valueHints[a]:0,hy=y.visits?valueHints[b]:0;
  if(valueTiesOnly&&hx!=hy)return hx>hy;
  return x.prior>y.prior;};
 sort(active.begin(),active.end(),order);report.chosen=report.root[active[0]].action;
 bool stopped=false;ContinuationTree continuation;
 while(active.size()>1&&!stopped){
  int stages=ceil(log2(double(active.size())));int batches=max(1,(budget.iterations-report.iterations)/max(1,int(active.size())*stages));
  for(int batch=0;batch<batches;batch++){
   if(report.iterations+(int)active.size()>budget.iterations||(report.iterations&&budget.milliseconds>0&&elapsed()>=budget.milliseconds)){stopped=true;break;}
   auto world=sampleBelief(obs,side,hypotheses,rng);auto common=rng.next();world.rng=Rng(common);int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
   // A separate stream preserves the hidden-world samples of the baseline.
   Rng plans(common^0x629cab1578ULL);int slot=plans.n(5),enemyPlan=variedPlans?(slot<3?0:slot-2):0;
   vector<double> outcomes;vector<ContinuationTree::Path> paths;
   for(int index:active){
    if(report.iterations&&budget.milliseconds>0&&elapsed()>=budget.milliseconds){stopped=true;break;}
    auto g=world;Rng rollout(common);apply(g,report.root[index].action);ContinuationTree::Path path;int level=1;
    while(g.winner<0){if(g.steps>500)throw runtime_error("long root race rollout");
     Act action;if(g.turn!=side)action=plannedOpponentReply(g,rollout,enemyStyle,enemyPlan);
     else if(searchContinuations&&level<budget.depth){action=continuation.choose(g,rollout,style,path,++level);}
     else action=fastV8(g,rollout,style,64);
     apply(g,action);
    }
    paths.push_back(std::move(path));
    outcomes.push_back(g.winner==2?.5:g.winner==side?1:0);report.iterations++;
   }
   // Discard an interrupted comparison rather than favouring the first moves.
   if(outcomes.size()!=active.size())break;
   for(int j=0;j<(int)active.size();j++){auto& e=report.root[active[j]];e.visits++;e.value+=outcomes[j];if(searchContinuations)continuation.learn(paths[j],outcomes[j]);}
  }
  sort(active.begin(),active.end(),order);report.chosen=report.root[active[0]].action;
  if(!stopped)active.resize((active.size()+1)/2);
 }
 report.nodes=1+continuation.nodes.size();report.depth=continuation.depth;report.milliseconds=elapsed();return report;
}
