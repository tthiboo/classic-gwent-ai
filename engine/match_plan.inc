// Research-only round policies. A plan is selected before its round's own
// decisions and stays fixed until the round changes. Terminal match outcomes,
// not the current row score, train the plan selector. No enemy identities enter.
struct MatchPlanSession { int round=-1,plan=0; };
struct MatchPlanNode { array<int,3> visits{};array<double,3> value{};int total=0; };
using MatchPlanPath=vector<pair<MatchPlanNode*,int>>;
vector<Scored> matchPlanOffsets(const Game& g,vector<Scored> ranked,int plan){
 int side=g.turn,other=1-side;
 if(!plan||g.pending>=0||g.opening||g.p[other].passed||eliminationPressure(g))return ranked;
 // In the decider resources have no later round to be saved for.
 if(g.p[0].wins==1&&g.p[1].wins==1)return ranked;
 int spent=max(0,g.p[side].drawn-int(g.p[side].hand.size()));
 for(auto& q:ranked){auto a=q.a;
  if(a.kind==3){
   if(plan==1&&g.p[side].hand.size()>=4&&spent>=3&&!ahead(g,side))q.score+=18;
   if(plan==2&&g.p[side].hand.size()>=4)q.score-=24;
   continue;
  }
  if(a.kind!=0||a.card<0||C[a.card].kind)continue;
  int flags=C[a.card].flags;
  if(flags&SPY)continue; // Harvesting resources is valuable in every plan.
  if(plan==1){
   // Keep flexible heroes and medics for the harder later round. Preserve
   // a complete known hand bond package instead of breaking it piecemeal.
   q.score-=.28*max(0,C[a.card].p)+(flags&HERO?3:0);
   if((flags&MED)&&g.p[0].wins+g.p[1].wins<2)q.score-=7;
   if(flags&BOND){int partners=0;for(int c:g.p[side].hand)partners+=C[c].name==C[a.card].name; if(partners>1)q.score-=3*(partners-1);}
  }else{
   // Pressure spends robust points now and attempts to win 2:0 after taking
   // round one. Search compares the cost against the preserving plan.
   if(flags&HERO)q.score+=3;
   if(flags&MED)q.score+=3;
  }
 }
 sort(ranked.begin(),ranked.end(),[](auto& a,auto& b){return a.score>b.score;});return ranked;
}
vector<Scored> rankMatchPlan(const Game& g,Rng& rng,int style,int plan){
 return matchPlanOffsets(g,rankMicro(g,rng,style),plan);
}
Act matchPlanPolicy(const Game& g,Rng& rng,int style,int plan){
 if(!plan)return fastV8(g,rng,style,64);
 auto catchPlan=catchUp(g,64);if(catchPlan.complete)return catchPlan.first;
 auto ranked=rankMatchPlan(g,rng,style,plan);auto safe=safeActionsV9(g);
 for(auto q:ranked)if(find(safe.begin(),safe.end(),q.a)!=safe.end())return q.a;
 throw runtime_error("empty match-plan policy");
}
struct MatchPlanTree {
 unordered_map<string,MatchPlanNode> nodes;
 string key(const Game& g)const{return informationKeyV9(g,g.turn,true);}
 int choose(const Game& g,MatchPlanSession& session,MatchPlanPath& path){
  if(session.round==g.round)return session.plan;
  session.round=g.round;session.plan=0;
  if(g.pending>=0||g.opening||(g.p[0].wins==1&&g.p[1].wins==1)||nodes.size()>=512)return 0;
  auto& n=nodes[key(g)];int best=0;double value=-1e100;
  for(int p=0;p<3;p++){
   // Visit all plans. Thereafter the terminal return directs exploration.
   if(!n.visits[p]){best=p;break;}
   double q=(n.value[p]+.5)/(n.visits[p]+1),u=q+.55*sqrt(log(n.total+1.0)/n.visits[p]);
   if(u>value){value=u;best=p;}
  }
  path.push_back({&n,best});return session.plan=best;
 }
 int replay(const Game& g,MatchPlanSession& session)const{
  if(session.round==g.round)return session.plan;
  session.round=g.round;session.plan=0;
  auto it=nodes.find(key(g));if(it==nodes.end()||it->second.total<12)return 0;
  const auto& n=it->second;int best=0;
  for(int p=1;p<3;p++)if(n.visits[p]>=4&&n.value[p]/n.visits[p]>n.value[best]/max(1,n.visits[best]))best=p;
  return session.plan=best;
 }
 void learn(const MatchPlanPath& path,double result){for(auto [n,p]:path){n->visits[p]++;n->value[p]+=result;n->total++;}}
};
