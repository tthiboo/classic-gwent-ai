#ifndef GWENT_HUMAN_SCENARIOS
#define GWENT_HUMAN_SCENARIOS
// Explicit test/model personas, not human demonstrations or training labels.
// Card availability belongs only to the acting hand. rankMicro determinizes
// the opponent before forecasting replies; these rules read public scores,
// public board units and hand sizes, never concealed opponent identities.
Act humanScenarioReply(const Game& g,Rng& rng,int style,int persona){
 auto closed=catchUp(g,64);if(closed.complete)return closed.first;
 if(g.pending>=0||g.opening)return fastV8(g,rng,style,64,true);
 if(persona==0)return fastV8(g,rng,style,64,true);
 if(persona==9)persona=g.round==0?1:g.round==1?2:3;
 auto ranked=rankMicro(g,rng,style,true);auto safe=safeActionsV9(g);
 int side=g.turn,other=1-side,spent=max(0,g.p[side].drawn-int(g.p[side].hand.size()));
 bool bleed=persona==7&&g.round==1&&g.p[side].wins==1&&!g.p[other].wins;
 for(auto& q:ranked){auto a=q.a;
  if(a.kind==3){
   if(persona==1&&g.p[side].hand.size()>3)q.score-=22;
   if(persona==2&&spent>=3&&g.p[side].hand.size()>=4)q.score+=12;
   if(bleed&&spent<3&&g.p[side].hand.size()>=4)q.score-=32;
   continue;
  }
  if(a.kind!=0)continue;auto c=C[a.card];
  if(persona==1&&!c.kind&&!(c.flags&SPY))q.score+=min(8.0,c.p*.5);
  if(persona==2&&(c.flags&(HERO|MED))&&g.round<2)q.score-=4;
  if(persona==4&&(c.flags&SPY||a.card==DEC))q.score+=9;
  if(persona==5&&(c.flags&(BOND|MUSTER|BERSERK|MARDROEM|MORALE|UHORN)))q.score+=5;
  if(bleed&&!c.kind&&!(c.flags&SPY))q.score+=4;
  if((persona==3||persona==8)&&responseCounter(a)){
   // Reward only counters which remove more opposing than friendly power.
   if(responseOpportunity(g,side,a.card))q.score+=persona==8?10:7;
  }
 }
 stable_sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.score>b.score;});
 vector<Scored> plausible;for(auto q:ranked)if(find(safe.begin(),safe.end(),q.a)!=safe.end())plausible.push_back(q);
 if(plausible.empty())throw runtime_error("no legal persona action");
 if(persona==6){int n=1;while(n<min(3,int(plausible.size()))&&plausible[n].score>=plausible[0].score-5)n++;return plausible[rng.n(n)].a;}
 return plausible.front().a;
}

#endif
