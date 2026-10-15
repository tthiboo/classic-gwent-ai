#define V7_NO_MAIN
#include "bot_v7.cpp"

struct OpeningAccess {
    bool emhyrAvailable=false;
    vector<int> spiesNow,spiesAfterRound;
    int enemyDrawPotential=0,ownDrawPotential=0;
};
OpeningAccess openingAccess(const Game& g,int side) {
    OpeningAccess info;int enemy=1-side;
    info.emhyrAvailable=g.p[enemy].leader==3&&!g.p[enemy].used;
    auto spy=[](int c){return revivable(c)&&(C[c].flags&SPY);};
    for(int c:g.p[side].grave)if(spy(c))info.spiesNow.push_back(c);
    info.spiesAfterRound=info.spiesNow;
    for(auto u:g.p[side].board)if(spy(u.id))info.spiesAfterRound.push_back(u.id);
    bool medic=false;for(int c:g.p[side].hand)medic|=bool(C[c].flags&MED);
    if(info.emhyrAvailable&&!info.spiesAfterRound.empty()) {
        info.enemyDrawPotential=min(2,(int)g.p[enemy].deck.size());
        // Winning Northern Realms draws one card before the next opening move.
        if(medic)info.ownDrawPotential=min(2,max(0,(int)g.p[side].deck.size()-(side==0?1:0)));
    }
    return info;
}
Game previewTurn(const Game& g,Act a,Rng& rng,double cv) {
    Game z=g;apply(z,a);finishChain(z,rng,cv,true);return z;
}
double bestKnownMedicBurst(Game g,int side,double cv) {
    // g.hand contains ONLY cards already known before the contemplated draw.
    double best=0;g.turn=side;g.pending=-1;Rng rng(7);
    for(auto a:legal(g))if(a.kind==0&&(C[a.card].flags&MED)) {
        auto z=previewTurn(g,a,rng,cv);
        int drawn=(int)z.p[side].hand.size()-(int)g.p[side].hand.size()+1;
        double gain=score(z,side)-score(z,1-side)-score(g,side)+score(g,1-side);
        best=max(best,gain+cv*drawn);
    }
    return best;
}
double nextRoundMedicValue(const Game& current,const Game& after,Act played,int side,double cv) {
    Game next=after;
    // Do not inspect the identities of cards drawn by a hypothetical current spy.
    // Only remaining PRE-EXISTING medics are eligible for the forecast.
    next.p[side].hand=current.p[side].hand;erase1(next.p[side].hand,played.card);
    for(auto& p:next.p) {
        for(auto u:p.board)p.grave.push_back(u.id);
        p.board.clear();p.passed=false;p.lhorn=0;
    }
    clearWeather(next);next.pending=-1;next.turn=side;next.round++;
    double value=bestKnownMedicBurst(next,side,cv);
    // If initiative is not secured, an unused opposing Emhyr can remove one
    // ordinary target first. This is a conservative forecast, not a claim that
    // the enemy must use Emhyr or that the next round has already been won.
    int enemy=1-side;
    bool startsOwn=current.p[enemy].passed&&ahead(after,side);
    if(!startsOwn&&current.p[enemy].leader==3&&!current.p[enemy].used) {
        set<int> targets(next.p[side].grave.begin(),next.p[side].grave.end());
        for(int c:targets)if(revivable(c)) {
            Game removed=next;erase1(removed.p[side].grave,c);
            value=min(value,bestKnownMedicBurst(removed,side,cv));
        }
    }
    return value;
}
// Public graveyard value: no concealed opponent cards are inspected.
double graveRecoveryValue(const Game& g,int side) {
 vector<double> values;
 for(int c:g.p[side].grave)if(revivable(c)) {
  double value=max(0.0,quality(c));
  if(C[c].flags&SPY)value=max(value,18.0*min(2,(int)g.p[side].deck.size())-C[c].p);
  values.push_back(value);
 }
 sort(values.rbegin(),values.rend());
 double value=values.empty()?0:values[0];
 if(values.size()>1)value+=.35*values[1];
 // Skellige's third-round return is automatic, unlike a speculative medic.
 if(g.p[side].faction==4&&g.round<2&&!values.empty())
  value+=min(2,(int)values.size())*accumulate(values.begin(),values.end(),0.0)/values.size();
 return value;
}
bool emptyCrach(const Game& g,Act a){return a.kind==1&&g.p[g.turn].leader==20&&g.p[0].grave.empty()&&g.p[1].grave.empty();}
#include "combo_plan.inc"
#ifndef GWENT_MATCH_POINT_REPLY
#define GWENT_MATCH_POINT_REPLY 0
#endif
vector<Scored> rankMicro(const Game& g,Rng& rng,int style,bool economicOpening=false) {
    auto ranked=rankV7(g,rng,style,15|PASS_REPLY|(economicOpening?CONCEDED_OPENING:0)|
                       (economicOpening&&GWENT_MATCH_POINT_REPLY?MATCH_POINT_REPLY:0));
    if(g.pending>=0)return ranked;
    int side=g.turn;double cv=style==0?20:style==1?15:25;
    // Preview uses only public board/grave information and own card counts.
    // The opponent's actual concealed cards cannot enter a feature.
    Game known=observation(g,side);
    vector<pair<int,double>> medicValues;double oldMedicMax=-1e100,newMedicMax=-1e100;
    for(int i=0;i<(int)ranked.size();i++) {
        auto a=ranked[i].a;
        if(side==comboPlanningSide)ranked[i].score+=comboSetupBonus(known,a);
        if(a.kind==1&&g.p[side].leader==20) {
            // Preserve the once-per-match ability unless denial beats the loss
            // of our own recovery options. Search can still evaluate recycling.
            ranked[i].score+=.6*(graveRecoveryValue(known,1-side)-graveRecoveryValue(known,side))-.65*cv;
        }
        if(a.kind==1&&g.p[side].leader==3) {
            Game stolen=known;apply(stolen,a);stolen.turn=side;
            double best=-1e100;
            for(auto play:legal(stolen))if(play.kind==0&&play.card==a.target) {
                auto z=previewTurn(stolen,play,rng,cv);
                int draws=(int)z.p[side].hand.size()-(int)stolen.p[side].hand.size()+1;
                double gain=score(z,side)-score(z,1-side)-score(stolen,side)+score(stolen,1-side);
                best=max(best,gain+cv*draws);
            }
            if(best>-1e99)ranked[i].score+=best-cv;
        }
        if(a.kind==0&&(C[a.card].flags&MED)&&hasRevive(g,side)&&
           g.p[0].wins+g.p[1].wins<2) {
            auto after=previewTurn(known,a,rng,cv);
            double future=nextRoundMedicValue(known,after,a,side,cv);
            oldMedicMax=max(oldMedicMax,ranked[i].score);
            double total=ranked[i].score+future;
            newMedicMax=max(newMedicMax,total);medicValues.push_back({i,total});
        }
    }
    // Change WHICH medic to spend without inventing an extra reason to play a
    // medic now. The family's maximum immediate score stays unchanged.
    for(auto [i,total]:medicValues)ranked[i].score=oldMedicMax+total-newMedicMax;
    sort(ranked.begin(),ranked.end(),[](auto& a,auto& b){return a.score>b.score;});return ranked;
}
bool emptyScorchLeader(const Game& g,Act a){if(a.kind!=1)return false;int l=g.p[g.turn].leader,row=l==2?1:l==7?2:l==17?0:-1;if(row<0)return false;Game after=observation(g,g.turn);auto count=after.p[1-g.turn].board.size();scorch(after,1-g.turn,row);return after.p[1-g.turn].board.size()==count;}
// Reject a pure self-hit, but retain trades that also remove enemy units.
bool selfOnlyScorch(const Game& g,Act a){
 if(a.kind!=0||a.card!=SCORCH)return false;
 Game z=g;scorch(z);int side=g.turn;
 return z.p[side].board.size()<g.p[side].board.size()&&z.p[1-side].board.size()==g.p[1-side].board.size();
}
// Root-only bounded forecast: can the sampled opponent catch us in <=3 cards?
// A pass should not donate a round while leaving them a large card advantage.
int cheapCatchAfterPass(const Game& g){
 int side=g.turn,enemy=1-side;
 if(g.pending>=0||g.p[enemy].passed||!ahead(g,side))return 99;
 vector<Game> beam{g};apply(beam[0],{3});int budget=180;Rng rng(713);
 for(int depth=1;depth<=3;depth++){
  vector<pair<double,Game>> next;
  for(const auto& node:beam)for(auto a:legal(node))if(a.kind==0){
   if(--budget<0)return 99;
   Game z=node;apply(z,a);finishChain(z,rng,20,true);
   if(z.winner==enemy||(z.round>g.round&&z.p[enemy].wins>g.p[enemy].wins)||ahead(z,enemy))return depth;
   if(z.winner<0&&z.round==g.round)next.push_back({double(score(z,enemy)-score(z,side)),std::move(z)});
  }
  sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.first>b.first;});
  beam.clear();for(int i=0;i<min(6,(int)next.size());i++)beam.push_back(std::move(next[i].second));
 }
 return 99;
}
// When losing this round eliminates us, a heuristic lead is not a pass certificate.
// Only a robust solver may override this while the opponent can still respond.
bool eliminationPressure(const Game& g){
 return g.pending<0&&g.p[1-g.turn].wins>=1&&!g.p[1-g.turn].passed;
}
Act fastV8(const Game& g,Rng& rng,int style=0,int planBudget=5000,bool economicOpening=false) {
    auto plan=catchUp(g,planBudget);if(plan.complete)return plan.first;
    auto ranked=rankMicro(g,rng,style,economicOpening);
    if(planBudget>64&&g.pending<0&&!g.p[1-g.turn].passed){
        int surplus=(int)g.p[1-g.turn].hand.size()-(int)g.p[g.turn].hand.size();
        if(surplus>0){int cost=cheapCatchAfterPass(g);if(cost<=surplus){
            for(auto& q:ranked)if(q.a.kind==3)q.score-=20.0*(surplus-cost+1);
            sort(ranked.begin(),ranked.end(),[](auto& a,auto& b){return a.score>b.score;});
        }}
    }
    ranked.erase(remove_if(ranked.begin(),ranked.end(),[&](auto q){return emptyScorchLeader(g,q.a)||emptyCrach(g,q.a)||selfOnlyScorch(g,q.a);}),ranked.end());
    if(eliminationPressure(g)&&any_of(ranked.begin(),ranked.end(),[](auto q){return q.a.kind!=3;}))
        ranked.erase(remove_if(ranked.begin(),ranked.end(),[](auto q){return q.a.kind==3;}),ranked.end());
    if(g.pending<0&&g.p[1-g.turn].passed&&g.p[g.turn].wins==1) {
        if(ahead(g,g.turn))return {3};
        for(auto q:ranked)if(q.a.kind!=3) {
            auto z=previewTurn(observation(g,g.turn),q.a,rng,20);
            if(ahead(z,g.turn))return q.a; // Enemy cannot answer; secure the match.
        }
    }
    return ranked.front().a;
}
// Small, bounded public-behaviour model. Keep every policy represented.
struct OpponentTempo {
 double continued=0,passed=0;
 void observe(bool pass){continued*=.96;passed*=.96;(pass?passed:continued)+=1;}
 array<double,3> weights()const{
  double shift=.36*(continued-passed)/(continued+passed+6);
  return {1.0/3,1.0/3+shift,1.0/3-shift};
 }
};
int sampleOpponentStyle(Rng& rng,const array<double,3>& weights){
 double total=0;for(double w:weights)total+=max(.15,w);
 double roll=rng.u()*total;for(int i=0;i<3;i++)if((roll-=max(.15,weights[i]))<=0)return i;
 return 2;
}
Act searchV8(const Game& observed,const vector<Hypothesis>& h,Rng& rng,int style=0,array<double,3> enemyWeights={1,1,1}) {
    auto first=sampleBelief(observed,observed.turn,h,rng);Act base=fastV8(first,rng,style);
    auto actions=legal(first);actions.erase(remove_if(actions.begin(),actions.end(),[&](Act a){return (emptyScorchLeader(first,a)||emptyCrach(first,a)||selfOnlyScorch(first,a)||(eliminationPressure(first)&&a.kind==3))&&!(a==base);}),actions.end());auto it=find(actions.begin(),actions.end(),base);rotate(actions.begin(),it,it+1);
    if(actions.size()==1)return base;
    auto playOut=[&](Game g,uint64_t seed,int opponent,int enemyStyle) {
        Rng r(seed);
        while(g.winner<0) {
            if(g.steps>500)throw runtime_error("long micro rollout");
            Act a;
            if(g.turn==observed.turn)a=fastV8(g,r,style,64);
            else if(opponent==2)a=fastV8(g,r,enemyStyle,64);
            else if(opponent==1)a=fastV7(g,r,enemyStyle,15,64);
            else a=policy(g,r,enemyStyle);
            apply(g,a);
        }
        return g.winner==observed.turn;
    };
    vector<int> wins(actions.size());
    for(int t=0;t<12;t++) {
        auto sampled=sampleBelief(observed,observed.turn,h,rng);auto seed=rng.next();int enemyStyle=sampleOpponentStyle(rng,enemyWeights);
        for(int j=0;j<(int)actions.size();j++) {
            Game z=sampled;apply(z,actions[j]);wins[j]+=playOut(z,seed,t%3,enemyStyle);
        }
    }
    int best=0;for(int j=1;j<(int)actions.size();j++)if(wins[j]>wins[best])best=j;
    if(!best)return base;
    Rng confirm(rng.next()^0x80d391cedea01012ULL);int difference=0;
    for(int t=0;t<64;t++) {
        auto sampled=sampleBelief(observed,observed.turn,h,confirm);auto seed=confirm.next();
        int enemy=confirm.n(3),enemyStyle=sampleOpponentStyle(confirm,enemyWeights);Game a=sampled,b=sampled;
        apply(a,base);apply(b,actions[best]);
        difference+=playOut(b,seed,enemy,enemyStyle)-playOut(a,seed,enemy,enemyStyle);
    }
    return difference/64.0>sqrt(2*log(20.0)/64)?actions[best]:base;
}
ReportV7 playV8(const Game& actual,const Deck& own,const vector<Deck>& lib,uint64_t seed,bool search=true,int style=0,array<double,3> enemyWeights={1,1,1}) {
    Game obs=observation(actual,actual.turn);
    // Resolve public, deterministic catch-up before terminal win/loss solvers:
    // tied losing evaluations must not burn every remaining weather card.
    auto catchPlan=catchUp(obs,5000);if(catchPlan.complete){ReportV7 result;result.chosen=catchPlan.first;result.catchComplete=true;return result;}
    auto h=compatible(obs,obs.turn,own,lib);
    if(h.empty())throw runtime_error("unsupported public evidence");
    Rng rng(seed);auto sampled=sampleBelief(obs,obs.turn,h,rng);ReportV7 out;
    bool same=true;for(auto& x:h)same&=x.unknown==h.front().unknown;
    if(same&&exactMove(sampled,out.chosen,100000)&&!emptyScorchLeader(obs,out.chosen)&&!emptyCrach(obs,out.chosen)&&!selfOnlyScorch(obs,out.chosen)){out.exact=true;return out;}
    if(!chanceRules(obs)&&robustMove(obs,h,out.chosen,100000)&&!emptyScorchLeader(obs,out.chosen)&&!emptyCrach(obs,out.chosen)&&!selfOnlyScorch(obs,out.chosen)){out.guaranteed=true;return out;}
    out.chosen=search?searchV8(obs,h,rng,style,enemyWeights):fastV8(sampled,rng,style);return out;
}
