#include "opening_evidence.inc"
// Validate a decision on fresh paired worlds using policies learned DURING the
// completed search, frozen before validation. Unknown observations fall back to
// fastV8. No validation outcome trains the policy it is currently evaluating.
int reviewDecisionV11(const Game& obs,const vector<Hypothesis>& h,const ReportV9& report,
 Act baseline,uint64_t seed,int style,array<double,3> weights){
 if(!report.continuations||obs.pending>=0||report.chosen==baseline||decisionClass(report.chosen)==decisionClass(baseline))return 0;
 auto safe=safeActionsV9(obs);if(find(safe.begin(),safe.end(),baseline)==safe.end())return 0;
 const auto& portfolio=*report.continuations;ResponseForecast forecast(&portfolio.response);
 auto index=[&](Act a){auto it=find(portfolio.roots.begin(),portfolio.roots.end(),a);return it==portfolio.roots.end()?-1:int(it-portfolio.roots.begin());};
 int chosen=index(report.chosen),original=index(baseline);if(chosen<0||original<0)return 0;
 Rng rng(seed^0xc03f51d94a725b61ULL);int side=obs.turn,context=learningContext(obs),completed=0;
 auto began=chrono::steady_clock::now();
 for(int trial=0;trial<8;trial++){
  if(trial&&chrono::duration<double,milli>(chrono::steady_clock::now()-began).count()>=35)break;
  auto world=sampleBelief(obs,side,h,rng);auto common=rng.next();world.rng=Rng(common);int enemyStyle=sampleOpponentStyle(rng,weights),enemyPlan=sampleRoundPlan(common,portfolio.enemyPlans);
  auto outcome=[&](int branch){auto g=world;Rng roll(common);MatchPlanSession session;if(portfolio.trees[branch].matchPlans)portfolio.trees[branch].plans.replay(g,session);apply(g,portfolio.roots[branch]);
   while(g.winner<0){if(g.steps>500)throw runtime_error("long continuation review");
    auto action=g.turn==side?portfolio.trees[branch].replayPolicy(g,roll,style,portfolio.trees[branch].matchPlans?portfolio.trees[branch].plans.replay(g,session):0):responseOpponentReply(g,roll,enemyStyle,enemyPlan,&forecast);
    apply(g,action);
   }
   return g.winner==2?.5:g.winner==side?1.0:0.0;
  };
  double alternative=outcome(chosen),before=outcome(original);
  double difference=alternative-before;
  decisionLearning.observe(context,decisionClass(report.chosen),decisionClass(baseline),difference);completed++;
 }
 return completed;
}
// Pick a supported challenger even when search agrees with its tactical
// fallback. Compare concrete choices only; duplicate-feature copies cannot
// teach this representation anything. Prefer the strongest searched challenger,
// not a deliberately bad move chosen to manufacture an easy training label.
int concreteChallengerV11(const Game& obs,const ReportV9& report,Act fallback){
 if(!report.continuations||obs.pending>=0)return -1;
 const auto& roots=report.root;int chosen=-1,best=-1;
 for(int i=0;i<(int)roots.size();i++)if(roots[i].action==report.chosen)chosen=i;
 if(chosen<0||roots[chosen].visits<2)return -1;
 auto features=moveFeatures(obs,report.chosen);auto safe=safeActionsV9(obs);
 auto eligible=[&](int i){auto a=roots[i].action;return !(a==report.chosen)&&roots[i].visits>=2&&decisionClass(a)==decisionClass(report.chosen)&&find(safe.begin(),safe.end(),a)!=safe.end()&&moveFeatures(obs,a)!=features;};
 for(int i=0;i<(int)roots.size();i++)if(eligible(i)){
  if(roots[i].action==fallback)return i;
  double q=(roots[i].value+.5)/(roots[i].visits+1);
  if(best<0||q>(roots[best].value+.5)/(roots[best].visits+1)||
    (q==(roots[best].value+.5)/(roots[best].visits+1)&&roots[i].prior>roots[best].prior))best=i;
 }
 return best;
}
struct ConcreteReviewV11 {Act alternative;int trials=0;double mean=0,standardError=0,prediction=0;bool accepted=false,corrected=false;vector<pair<double,double>> outcomes;};
ConcreteReviewV11 reviewConcreteV11(const Game& obs,const vector<Hypothesis>& h,const ReportV9& report,
 Act fallback,uint64_t seed,int style,array<double,3> weights,double milliseconds=35){
 ConcreteReviewV11 result;int challenger=concreteChallengerV11(obs,report,fallback);if(challenger<0||milliseconds<=0)return result;
 const auto& p=*report.continuations;ResponseForecast forecast(&p.response);auto where=find(p.roots.begin(),p.roots.end(),report.chosen);
 if(where==p.roots.end())return result;int chosen=where-p.roots.begin();
 auto rival=find(p.roots.begin(),p.roots.end(),report.root[challenger].action);
 if(rival==p.roots.end())return result;challenger=rival-p.roots.begin();result.alternative=p.roots[challenger];
 Rng rng(seed^0x921c531af084ab37ULL);int side=obs.turn;double sum=0,squares=0;
 auto began=chrono::steady_clock::now();
 for(int trial=0;trial<8;trial++){
  if(chrono::duration<double,milli>(chrono::steady_clock::now()-began).count()>=milliseconds)break;
  auto world=sampleBelief(obs,side,h,rng);auto common=rng.next();world.rng=Rng(common);
  int enemyStyle=sampleOpponentStyle(rng,weights),enemyPlan=sampleRoundPlan(common,p.enemyPlans);
  auto outcome=[&](int branch){auto g=world;Rng roll(common);MatchPlanSession session;if(p.trees[branch].matchPlans)p.trees[branch].plans.replay(g,session);apply(g,p.roots[branch]);
   while(g.winner<0){if(g.steps>500)throw runtime_error("long concrete review");
    apply(g,g.turn==side?p.trees[branch].replayPolicy(g,roll,style,p.trees[branch].matchPlans?p.trees[branch].plans.replay(g,session):0):responseOpponentReply(g,roll,enemyStyle,enemyPlan,&forecast));
   }return g.winner==2?.5:g.winner==side?1.0:0.0;
  };
  double own=outcome(chosen),other=outcome(challenger),difference=own-other;result.outcomes.push_back({own,other});sum+=difference;squares+=difference*difference;result.trials++;
 }
 if(result.trials<6)return result;
 result.mean=sum/result.trials;result.standardError=sqrt(max(0.0,(squares-sum*sum/result.trials)/(result.trials-1))/result.trials);
 result.prediction=moveLearning.prediction(obs,report.chosen)-moveLearning.prediction(obs,result.alternative);
 result.accepted=moveLearning.observe(obs,report.chosen,result.alternative,result.mean,result.standardError,result.trials);
 // An independent check can correct this decision without waiting for a
 // global model to mature. Safety/exact filters remain authoritative. Evidence
 // uses approximate frozen policies; the whole policy still needs qualification.
 result.corrected=result.mean<-(.04+2*result.standardError);
 return result;
}
ReportV7 playV11(const Game& actual,const Deck& own,const vector<Deck>& lib,uint64_t seed,
 bool search=true,int style=0,array<double,3> weights={1,1,1},BudgetV9 budget={2048,250,18},array<double,3> enemyPlans={1,0,0},bool concreteLessons=false,bool moveHints=true,bool verifiedCorrections=false,bool learnedPairPriors=false,bool learnedPairFuture=true,bool matchPlans=false,bool lockedPlans=false,bool characterTies=false,bool characterPlans=false,const ResponseModel* response=nullptr,const VerifiedLessons* lessons=nullptr){
 lastSearchV9={};if(search&&lessons)if(auto cached=verifiedContinuation(actual,lessons)){ReportV7 result;result.chosen=*cached;result.guaranteed=true;lastSearchV9.verifiedLessonMatches=1;lastSearchV9.verifiedLessonProof=true;return result;}
 auto fallback=playV8(actual,own,lib,seed,false,style,weights);
 if(!search||fallback.exact||fallback.guaranteed||fallback.catchComplete)return fallback;
 if(lessons){int matches=0;auto proof=verifiedLessonProof(actual,fallback.chosen,lessons,matches);if(proof){fallback.chosen=*proof;fallback.guaranteed=true;lastSearchV9.verifiedLessonMatches=matches;lastSearchV9.verifiedLessonProof=true;return fallback;}if(matches)lessons=nullptr;}
 auto obs=observation(actual,actual.turn);auto h=compatible(obs,obs.turn,own,lib);
 lastSearchV9=parallelRoot(obs,h,seed,style,weights,budget,true,3,true,enemyPlans,concreteLessons&&moveHints,learnedPairPriors,learnedPairFuture,matchPlans,lockedPlans,false,nullptr,6,false,nullptr,false,characterPlans,response,lessons);
 if(characterTies){auto began=chrono::steady_clock::now();chooseCharacterTie(obs,lastSearchV9,style);
  lastSearchV9.characterTieMilliseconds=chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();}
 reviewOpeningPass(obs,h,seed,style,lastSearchV9);
 auto start=chrono::steady_clock::now();
 lastSearchV9.reviewTrials=reviewDecisionV11(obs,h,lastSearchV9,fallback.chosen,seed,style,weights);
 if(concreteLessons){
  double remaining=35-chrono::duration<double,milli>(chrono::steady_clock::now()-start).count();
  auto review=reviewConcreteV11(obs,h,lastSearchV9,fallback.chosen,seed,style,weights,remaining);
  lastSearchV9.concreteReviewTrials=review.trials;lastSearchV9.concreteLesson=review.accepted;lastSearchV9.concreteCorrected=verifiedCorrections&&review.corrected;
  lastSearchV9.concreteMargin=review.mean;lastSearchV9.concreteStandardError=review.standardError;
  lastSearchV9.concreteOriginal=lastSearchV9.chosen;lastSearchV9.concreteAlternative=review.alternative;lastSearchV9.concretePrediction=review.prediction;lastSearchV9.concreteOutcomes=std::move(review.outcomes);
  if(lastSearchV9.concreteCorrected)lastSearchV9.chosen=review.alternative;
 }
 lastSearchV9.reviewMilliseconds=chrono::duration<double,milli>(chrono::steady_clock::now()-start).count();
 fallback.chosen=lastSearchV9.chosen;return fallback;
}

// Qualified full-budget round-plan portfolio. Exact and public catch-up proofs
// retain precedence; the research locked-plan and learned models stay disabled.
ReportV7 playV12(const Game& actual,const Deck& own,const vector<Deck>& lib,uint64_t seed,
 bool search=true,int style=0,array<double,3> weights={1,1,1},BudgetV9 budget={2048,250,18},bool characterTies=true,bool characterPlans=false,const ResponseModel* response=nullptr,const VerifiedLessons* lessons=nullptr){
 auto result=playV11(actual,own,lib,seed,search,style,weights,budget,{1,0,0},false,true,false,false,true,true,false,characterTies,characterPlans,response,lessons);
 // Exact/fallback/learned return paths must obey the same opening candidates
 // as the search. The old pristine-empty-board check missed a player's reply.
 auto safe=safeActionsV9(observation(actual,actual.turn));
 if(!safe.empty()&&find(safe.begin(),safe.end(),result.chosen)==safe.end()){
  Rng priority(seed^0x519ddULL);auto ranked=rankMicro(observation(actual,actual.turn),priority,style);
  Act replacement=safe.front();for(auto q:ranked)if(find(safe.begin(),safe.end(),q.a)!=safe.end()){replacement=q.a;break;}
  if(result.chosen.kind==3)lastSearchV9.passOverridden=true;
  result.chosen=replacement;result.guaranteed=false;lastSearchV9.chosen=replacement;
 }
 // Approximate continuations cannot certify a cost-free concession before
 // either player has acted. Keep initiative when a safe unit can be played.
 // This final entry-point guard also covers early fallback/proof returns.
 if(search&&openingEvidenceEnabled&&result.chosen.kind==3&&pristineOpening(actual)){
  auto safe=safeActionsV9(observation(actual,actual.turn));vector<Act> units;
  for(auto a:safe)if(a.kind==0&&!C[a.card].kind)units.push_back(a);
  if(!units.empty()){
   Act best=units.front();double value=-1,prior=-1;
   for(auto e:lastSearchV9.root)if(find(units.begin(),units.end(),e.action)!=units.end()){
    double q=(e.value+.5)/(e.visits+1);
    if(q>value||(q==value&&e.prior>prior)){best=e.action;value=q;prior=e.prior;}
   }
   result.chosen=best;result.guaranteed=false;lastSearchV9.chosen=best;lastSearchV9.passOverridden=true;
  }
 }
 return result;
}
