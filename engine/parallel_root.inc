#include <thread>
#include <atomic>
#include "character_search_priority.inc"
// Independent root branches run concurrently. A branch's future information
// tree belongs exclusively to that branch; workers never mutate shared search
// statistics, the card catalogue, behavioural memory or production files.
ReportV9 parallelRoot(const Game& actual,const vector<Hypothesis>& hypotheses,uint64_t seed,
 int style=0,array<double,3> enemyWeights={1,1,1},BudgetV9 budget={2048,250,18},bool futureSearch=false,int workers=3,bool retainPlans=false,array<double,3> enemyPlans={1,0,0},bool concreteLessons=false,bool learnedPairPriors=false,bool learnedPairFuture=true,bool matchPlans=false,bool lockedPlans=false,bool captureTransfer=false,const TrajectoryValue* trajectoryValue=nullptr,int leafDepth=6,bool blendTrajectory=false,const SearchActor* actor=nullptr,bool actorPriorsOnly=false,bool characterPlans=false,const ResponseModel* response=nullptr,const VerifiedLessons* lessons=nullptr){
 if(budget.iterations<1||workers<1||workers>3)throw runtime_error("invalid parallel search budget");
 if(trajectoryValue&&(!trajectoryValue->enabled||leafDepth<1||leafDepth>budget.depth))throw runtime_error("invalid trajectory leaf config");
 if(characterPlans&&(trajectoryValue||style<0||style>2))throw runtime_error("character comparison requires terminal match returns and a valid profile");
 if(actor&&!actor->enabled)throw runtime_error("invalid future actor");
 // A zero residual is the actual legacy search, including rollout fallback,
 // tree exploration, macro plans and random-stream consumption.
 if(actor&&!actor->hasResidual())actor=nullptr;
 ResponseForecast responseForecast(response);auto began=chrono::steady_clock::now();auto elapsed=[&](){return chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();};
 auto obs=observation(actual,actual.turn);int side=obs.turn;Rng rng(seed);ReportV9 report;report.responseForecastEnabled=response&&response->enabled;if(report.responseForecastEnabled)for(auto b:response->habits)report.responseActiveHabits+=b.active();
 auto safe=safeActionsV9(obs);auto priorWorld=sampleBelief(obs,side,hypotheses,rng);auto ranked=rankMicro(priorWorld,rng,style);double total=0;
 auto hints=concreteLessons?moveLearning.adjustments(obs,safe):vector<double>(safe.size());
 for(auto a:safe){int rank=0;while(rank<(int)ranked.size()&&!(ranked[rank].a==a))rank++;
  double prior=(.12/safe.size()+.88*exp(-.55*rank))*decisionLearning.multiplier(learningContext(obs),decisionClass(a));
  total+=prior;report.root.push_back({a,prior});
 }
 if(concreteLessons){applyMoveHints(report.root,hints);report.concreteHintNodes+=any_of(hints.begin(),hints.end(),[](double x){return x!=0;});}
 if(learnedPairPriors){report.pairHintNodes+=applyPairPriors(report.root,obs,true);total=0;for(auto& e:report.root)total+=e.prior;}
 report.verifiedLessonMatches=applyVerifiedLessonHints(report.root,obs,lessons);if(report.verifiedLessonMatches){total=0;for(auto e:report.root)total+=e.prior;}
 for(auto& e:report.root)e.prior/=total;
 // Research alternative: learned priors guide tree exploration, while leaf
 // rollouts and capacity fallback retain the mature policy used by baseline.
 vector<ContinuationTree> trees(report.root.size());for(auto& tree:trees){tree.actor=actor;tree.actorPriorsOnly=actorPriorsOnly;tree.captureTransfer=captureTransfer;tree.matchPlans=matchPlans;tree.roundResources=matchPlans||enemyPlans[1]>0||enemyPlans[2]>0;tree.concreteLessons=concreteLessons;tree.learnedPairPriors=learnedPairPriors&&learnedPairFuture;}

 vector<int> active(report.root.size());iota(active.begin(),active.end(),0);
 vector<string> outcomesSeen(report.root.size());vector<array<double,3>> characterReturns(report.root.size());vector<double> tiePrior;for(auto e:report.root)tiePrior.push_back(e.prior);
 auto reorder=[&](){if(characterPlans)tiePrior=characterSearchPriorities(obs,report,active,outcomesSeen,style,&characterReturns);};
 auto order=[&](int a,int b){auto& x=report.root[a];auto& y=report.root[b];double qx=(x.value+.5)/(x.visits+1),qy=(y.value+.5)/(y.visits+1);
  if(qx!=qy)return qx>qy;
  return tiePrior[a]>tiePrior[b];};
 sort(active.begin(),active.end(),order);report.chosen=report.root[active[0]].action;bool stopped=false;
 // Separate planning from evaluation: discovery results train continuations,
 // but do not count as evidence for a root action. Round-plan statistics are
 // frozen before fresh root-comparison worlds; validation never retrains them.
 auto runBatch=[&](bool discovery)->bool{
   auto world=sampleBelief(obs,side,hypotheses,rng);auto common=rng.next();world.rng=Rng(common);int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
   int enemyPlan=sampleRoundPlan(common,enemyPlans);
   vector<double> outcomes(active.size(),-1);vector<array<double,3>> characterValues(active.size());vector<ContinuationTree::Path> paths(active.size());vector<MatchPlanPath> planPaths(active.size());vector<exception_ptr> failures(active.size());atomic<int> next{0},completed{0},leaves{0};
   auto work=[&](){for(;;){int slot=next.fetch_add(1);if(slot>=(int)active.size())return;
    if((report.iterations||completed.load())&&budget.milliseconds>0&&elapsed()>=budget.milliseconds)return;
    try{int index=active[slot];auto g=world;Rng roll(common);double stateEstimate=-1;MatchPlanSession planSession;CharacterPlanTrace characterTrace(g,side);
    auto step=[&](Act a){if(characterPlans)characterTrace.before(g,a);apply(g,a);if(characterPlans)characterTrace.after(g);};
    if(matchPlans){if(!lockedPlans||discovery)trees[index].plans.choose(g,planSession,planPaths[slot]);else trees[index].plans.replay(g,planSession);}step(report.root[index].action);int level=1;
    while(g.winner<0){if(g.steps>500)throw runtime_error("long parallel rollout");Act move;
     // Only our live normal decision states match the training domain.
     if(trajectoryValue&&stateEstimate<0&&g.turn==side&&level>=leafDepth&&g.pending<0&&!g.opening){
      stateEstimate=trajectoryValue->predict(g,side,style);leaves.fetch_add(1);
      if(!blendTrajectory){outcomes[slot]=stateEstimate;break;}
     }
     if(g.turn!=side)move=responseOpponentReply(g,roll,enemyStyle,enemyPlan,&responseForecast);
     else {int plan=matchPlans?((!lockedPlans||discovery)?trees[index].plans.choose(g,planSession,planPaths[slot]):trees[index].plans.replay(g,planSession)):0;
      if(futureSearch&&level<budget.depth)move=trees[index].choose(g,roll,style,paths[slot],++level,plan);
      else move=actor&&!actorPriorsOnly?actorFuturePolicy(g,roll,style,plan,actor):(matchPlans?matchPlanPolicy(g,roll,style,plan):fastV8(g,roll,style,64));
     }
     step(move);
    }
    if(g.winner>=0){double terminal=g.winner==2?.5:g.winner==side?1:0;
     // Conservative research scope: actual completed simulation dominates.
     // One frozen intermediate estimate, no retuned action-prior weights.
     outcomes[slot]=blendTrajectory&&stateEstimate>=0?.75*terminal+.25*stateEstimate:terminal;
    }if(characterPlans)characterValues[slot]=characterTrace.value;completed.fetch_add(1);
    }catch(...){failures[slot]=current_exception();return;}
   }};
   // The calling thread also works; at most two extra workers, leaving CPU
   // capacity for the game UI. Join before updating ANY shared root statistics.
   vector<thread> pool;int available=max(1,int(thread::hardware_concurrency())-1);int count=min({workers,(int)active.size(),available});
   try{for(int i=1;i<count;i++)pool.emplace_back(work);work();}
   catch(...){for(auto& thread:pool)thread.join();throw;}
   for(auto& thread:pool)thread.join();for(auto& failure:failures)if(failure)rethrow_exception(failure);
   report.iterations+=completed.load();report.trajectoryLeaves+=leaves.load();
   if(completed.load()!=(int)active.size())return false;
   for(int j=0;j<(int)active.size();j++){auto& e=report.root[active[j]];if(!discovery){e.visits++;e.value+=outcomes[j];outcomesSeen[active[j]].push_back(char('0'+int(outcomes[j]*2)));if(characterPlans)for(int k=0;k<3;k++)characterReturns[active[j]][k]+=characterValues[j][k];}if(futureSearch)trees[active[j]].learn(paths[j],outcomes[j]);if(matchPlans&&(!lockedPlans||discovery))trees[active[j]].plans.learn(planPaths[j],outcomes[j]);}
   return true;
 };
 if(matchPlans&&lockedPlans){
  for(int batch=0;batch<12;batch++){
   if(report.iterations+int(active.size())>budget.iterations/4)break;
   if(batch&&budget.milliseconds>0&&elapsed()>=budget.milliseconds*.2)break;
   if(!runBatch(true)){stopped=true;break;}
  }
 }
 while(active.size()>1&&!stopped){
  int stages=ceil(log2(double(active.size())));int batches=max(1,(budget.iterations-report.iterations)/max(1,int(active.size())*stages));
  // The simulation ceiling is optimistic on a busy machine. Split remaining
  // wall time between elimination stages too, so a deadline cannot spend the
  // whole search on a broad first sweep and starve the final comparisons.
  double stageDeadline=budget.milliseconds>0?elapsed()+max(0.0,budget.milliseconds-elapsed())/stages:0;
  for(int batch=0;batch<batches;batch++){
   if(batch&&stageDeadline>0&&elapsed()>=stageDeadline)break;
   if(report.iterations+(int)active.size()>budget.iterations||(report.iterations&&budget.milliseconds>0&&elapsed()>=budget.milliseconds)){stopped=true;break;}
   if(!runBatch(false)){stopped=true;break;}
  }
  reorder();sort(active.begin(),active.end(),order);report.chosen=report.root[active[0]].action;
  if(!stopped)active.resize((active.size()+1)/2);
 }
 report.nodes=1;report.depth=1;for(auto& tree:trees){report.nodes+=tree.nodes.size();report.actorNodes+=tree.actorNodes;report.depth=max(report.depth,tree.depth);report.concreteHintNodes+=tree.concreteHintNodes;report.pairHintNodes+=tree.pairHintNodes;report.matchPlanNodes+=tree.plans.nodes.size();for(const auto& entry:tree.plans.nodes)for(int plan=0;plan<3;plan++)report.matchPlanTrials[plan]+=entry.second.visits[plan];}
 if(futureSearch&&retainPlans){report.continuations=make_shared<ContinuationPortfolio>();report.continuations->enemyPlans=enemyPlans;if(response)report.continuations->response=*response;for(auto e:report.root)report.continuations->roots.push_back(e.action);report.continuations->trees=std::move(trees);}
 report.milliseconds=elapsed();return report;
}
