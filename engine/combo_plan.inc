// Bounded, public forward planning of a deployment package. These are setup
// estimates, not free extra turns or a guarantee that the opponent cannot reply.
// Rollout/race opponents are unchanged during paired qualification.
int comboPlanningSide=-1;
bool deployment(Act a){return a.kind==0&&(a.card==HORN||a.card==MARD||(!C[a.card].kind&&!(C[a.card].flags&(SPY|MED|HERO|DRAGON|RSCORCH|SSCORCH|GSCORCH))));}
int bondPackage(int card){return C[card].group>=0?C[card].group:card;}
double comboSetupBonus(const Game& observed,Act first){
 if(!deployment(first))return 0;
 int side=observed.turn,row=first.row;
 if(row<0)return 0;
 // Do not reward laying an empty Horn/Mardroeme as a tempo move.
 if(C[first.card].kind&&none_of(observed.p[side].board.begin(),observed.p[side].board.end(),[&](auto u){return u.row==row;}))return 0;
 auto connected=[&](Act a){
  if(!deployment(a)||a.row!=row)return false;
  auto f=C[first.card].flags,b=C[a.card].flags;
  return first.card==HORN||a.card==HORN||(f|b)&(MORALE|UHORN)||
   ((f&BOND)&&(b&BOND)&&bondPackage(first.card)==bondPackage(a.card))||
   ((first.card==MARD||f&MARDROEM)&&(b&BERSERK))||
   ((a.card==MARD||b&MARDROEM)&&(f&BERSERK));
 };
 Game start=observed,after=start;apply(after,first);
 if(after.winner>=0||after.round!=start.round||after.pending>=0)return 0;
 after.turn=side;double base=score(start,side),gain=score(after,side)-base,best=0;
 auto follow=legal(after);int tested=0;
 for(auto second:follow)if(connected(second)){
  if(++tested>12)break;
  Game pair=after;apply(pair,second);if(pair.pending>=0||pair.round!=start.round||pair.winner>=0)continue;
  // Compare with the same later play without the proposed setup card. A strong
  // unrelated unit alone cannot masquerade as a discovered combination.
  Game without=start;apply(without,second);if(without.pending>=0||without.round!=start.round)continue;
  double pairGain=score(pair,side)-base,independent=score(without,side)-base;
  if(pairGain>gain+independent)best=max(best,pairGain/2-gain);
  // Three-card Tight Bond is precisely where one-ply values miss the payoff.
  if(!(C[first.card].flags&BOND)||bondPackage(first.card)!=bondPackage(second.card))continue;
  pair.turn=without.turn=side;
  for(auto third:legal(pair))if(deployment(third)&&third.row==row&&(C[third.card].flags&BOND)&&bondPackage(third.card)==bondPackage(first.card)){
   Game trio=pair,other=without;apply(trio,third);apply(other,third);
   double total=score(trio,side)-base;
   if(total>gain+score(other,side)-base)best=max(best,total/3-gain);
  }
 }
 return max(0.0,best);
}
