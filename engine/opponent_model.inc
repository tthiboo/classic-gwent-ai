// Bayesian mixture over existing rollout styles, with forgetting and a uniform
// safety component. This estimates behaviour, not the identity of hidden cards.
struct StyleBelief {
 array<double,3> evidence{0,0,0};
 int observations=0;
 array<double,3> weights()const {
  double peak=*max_element(evidence.begin(),evidence.end()),sum=0;array<double,3> w;
  for(int k=0;k<3;k++){w[k]=exp(evidence[k]-peak);sum+=w[k];}
  for(auto& x:w)x=.2/3+.8*x/sum;
  return w;
 }
 void observe(array<double,3> likelihood){
  for(auto x:likelihood)if(!isfinite(x)||x<=0)return;
  auto bounds=minmax_element(likelihood.begin(),likelihood.end());
  if(*bounds.second-*bounds.first<1e-6)return; // indistinguishable styles teach nothing
  for(int k=0;k<3;k++)evidence[k]=.94*evidence[k]+.5*log(max(.02,likelihood[k]));
  double peak=*max_element(evidence.begin(),evidence.end());
  for(auto& x:evidence)x=max(-6.0,x-peak);
  observations=min(100000,observations+1);
 }
};
// Learn willingness to spend cards in the disposable first round. A strong
// generalist prior and an exploration floor prevent one pass from declaring
// a fixed personality. Forced decisions and indistinguishable predictions
// supply no evidence. The existing tactical-style model remains independent.
struct RoundPlanBelief {
 array<double,3> evidence{0,0,0};int observations=0;
 array<double,3> weights()const {
  const array<double,3> prior{.8,.1,.1};array<double,3> w;double sum=0;
  for(int k=0;k<3;k++){w[k]=prior[k]*exp(evidence[k]);sum+=w[k];}
  for(double& x:w)x=.06/3+.94*x/sum;
  return w;
 }
 array<double,3> forecastWeights()const {
  auto posterior=weights();
  // Do not activate the previously rejected unconditional plan mixture.
  // Several distinguishing observations must support an alternative plan.
  if(observations<3||max(posterior[1],posterior[2])<.25)return {1,0,0};
  return posterior;
 }
 void observe(array<double,3> likelihood){
  for(double x:likelihood)if(!isfinite(x)||x<=0||x>1)return;
  auto bounds=minmax_element(likelihood.begin(),likelihood.end());if(*bounds.second-*bounds.first<1e-6)return;
  for(int k=0;k<3;k++)evidence[k]=.94*evidence[k]+.5*log(max(.02,likelihood[k]));
  double peak=*max_element(evidence.begin(),evidence.end());for(double& x:evidence)x=max(-6.0,x-peak);
  observations=min(100000,observations+1);
 }
};
array<double,3> roundPlanLikelihood(const Game& publicObservation,const vector<Hypothesis>& h,Act played,uint64_t seed,array<double,3> styles={1,1,1}){
 array<double,3> result{};Rng worlds(seed);int observer=publicObservation.turn,actor=1-observer;
 double total=0;for(double x:styles){if(!isfinite(x)||x<0)throw runtime_error("invalid style mixture");total+=x;}
 if(total<=0)throw runtime_error("empty style mixture");
 for(int trial=0;trial<6;trial++){
  auto g=sampleBelief(publicObservation,observer,h,worlds);g.turn=actor;auto shared=worlds.next();
  for(int style=0;style<3;style++){
   Rng ranked(shared);auto base=fastV8(g,ranked,style,64,true);
   // Every plan formerly recomputed the SAME base reply from the SAME RNG.
   // Reuse it, then give each adjustment an independent copy of its post-reply
   // stream. Per-plan summation order and all predictions remain exact.
   for(int plan=0;plan<3;plan++){Rng rng=ranked;auto predicted=strategicOpponentPlan(g,rng,style,plan,base);
    result[plan]+=styles[style]/total*((predicted.kind==3)==(played.kind==3)?.9:.1)/6;
   }
  }
 }
 return result;
}
// Compare the public type of decision, not an exact card the bot cannot know was
// available. Differently named units can express the same strategic intention.
// The old nine-way classifier grouped Geralt, a one-point infantry unit and a
// full summon/bond commitment together. It could not learn from choosing one
// of these over another. Keep special-card roles, distinguish public unit roles.
int opponentIntent(Act a){
 int category=decisionClass(a);if(category!=8||a.kind!=0)return category;
 auto c=C[a.card];if(c.flags&HERO)return 9;if(c.flags&BOND)return 10;
 if(c.flags&MUSTER)return 11;return c.p<=4?12:13;
}
array<double,3> styleLikelihood(const Game& publicObservation,const vector<Hypothesis>& h,Act played,uint64_t seed,bool detailedIntent=false) {
 array<double,3> result{0,0,0};Rng worlds(seed);
 const int observer=publicObservation.turn,actor=1-observer,category=detailedIntent?opponentIntent(played):decisionClass(played);
 for(int trial=0;trial<6;trial++) {
  auto g=sampleBelief(publicObservation,observer,h,worlds);g.turn=actor;
  auto sharedSeed=worlds.next();auto allowed=safeActionsV9(g);
  for(int style=0;style<3;style++) {
   Rng rng(sharedSeed);auto ranked=rankMicro(g,rng,style);
   double peak=-1e100;
   for(auto q:ranked)if(find(allowed.begin(),allowed.end(),q.a)!=allowed.end())peak=max(peak,q.score);
   double total=0,matching=0;
   for(auto q:ranked)if(find(allowed.begin(),allowed.end(),q.a)!=allowed.end()) {
    double w=exp(max(-20.0,min(0.0,(q.score-peak)/12.0)));
    total+=w;if((detailedIntent?opponentIntent(q.a):decisionClass(q.a))==category)matching+=w;
   }
   // A human can take an action outside our models, including a tactical error.
   result[style]+=.05+.95*(total?matching/total:0);
  }
 }
 for(auto& x:result)x/=6;
 return result;
}
