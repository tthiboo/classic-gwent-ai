// Contextual public-response models. The default remains the qualified mature
// opponent. Only repeated, distinguishing evidence activates an alternative;
// even then most simulated replies retain the mature policy.
struct ResponseBelief {
 array<double,3> evidence{};int observations=0;
 array<double,3> weights()const{array<double,3> w{};double sum=0;for(int k=0;k<3;k++){w[k]=(k?.1:.8)*exp(evidence[k]);sum+=w[k];}for(double& x:w)x=.06/3+.94*x/sum;return w;}
 void observe(array<double,3> likelihood){
  for(double x:likelihood)if(!isfinite(x)||x<=0||x>1)return;
  auto limits=minmax_element(likelihood.begin(),likelihood.end());if(*limits.second-*limits.first<.01)return;
  for(int k=0;k<3;k++)evidence[k]=.85*evidence[k]+.7*log(max(.02,likelihood[k]));
  double peak=*max_element(evidence.begin(),evidence.end());for(double& x:evidence)x=max(-8.0,x-peak);observations=min(100000,observations+1);
 }
 bool active()const{auto w=weights();return observations>=6&&max(w[1],w[2])>.35;}
};
struct ResponseModel {array<ResponseBelief,4> habits;bool enabled=false;};
struct ResponseForecast {array<array<double,3>,4> weights{};unsigned mask=0;ResponseForecast(const ResponseModel* m=nullptr){if(m&&m->enabled)for(int k=0;k<4;k++)if(m->habits[k].active()){mask|=1u<<k;weights[k]=m->habits[k].weights();}}};
array<int,3> responseRows(const Game& g,int side){array<int,3> rows{};auto p=powers(g,side);for(int i=0;i<int(p.size());i++)if(!(C[g.p[side].board[i].id].flags&HERO))rows[g.p[side].board[i].row]+=p[i];return rows;}
bool responseVoluntary(const Game& g,int actor){return g.winner<0&&g.pending<0&&!g.opening&&!g.p[1-actor].passed&&g.p[actor].hand.size()>=3;}
int responseLoadedRow(const Game& g,int side){auto rows=responseRows(g,side);int row=max_element(rows.begin(),rows.end())-rows.begin();return rows[row]>=6?row:-1;}
bool responseCounter(Act a){return a.kind==0&&(C[a.card].weather||a.card==SCORCH||(C[a.card].flags&(DRAGON|RSCORCH|SSCORCH|GSCORCH)));}
bool responseThreat(const Game& g,int actor){auto rows=responseRows(g,1-actor);return *max_element(rows.begin(),rows.end())>=10;}
vector<Scored> responseOffsets(const Game& g,vector<Scored> ranked,int axis,int mode){
 if(!mode)return ranked;
 if(axis<2)return matchPlanOffsets(g,std::move(ranked),mode);
 int loaded=responseLoadedRow(g,g.turn);
 bool late=g.round==0?max(0,g.p[g.turn].drawn-int(g.p[g.turn].hand.size()))>=3:min(g.p[0].hand.size(),g.p[1].hand.size())<=4;
 for(auto& q:ranked){auto a=q.a;
  if(axis==2&&a.kind==0&&C[a.card].row==3&&!(C[a.card].flags&SPY)&&loaded>=0&&loaded<2)q.score+=(a.row==loaded?1:-1)*(mode==1?5:-5);
  if(axis==3&&responseCounter(a))q.score+=mode==1?8:(late?8:-8);
 }
 sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.score>b.score;});return ranked;
}
int responseCategory(const Game& g,Act a,int axis){
 if(axis<2){if(a.kind==3)return 0;if(a.kind==0&&!C[a.card].kind&&!(C[a.card].flags&SPY)&&((C[a.card].flags&(HERO|MED|MUSTER|BOND))||C[a.card].p>=8))return 2;return 1;}
 if(axis==2){if(a.kind!=0||C[a.card].row!=3||C[a.card].flags&SPY)return -1;int row=responseLoadedRow(g,g.turn);return row<0||row>1?-1:a.row==row?1:2;}
 return responseCounter(a)?1:2;
}
array<double,3> responseLikelihood(const Game& obs,const vector<Hypothesis>& h,Act played,int axis,uint64_t seed){
 array<double,3> likelihood{};Rng worlds(seed);int actor=1-obs.turn;
 for(int trial=0;trial<4;trial++){auto g=sampleBelief(obs,obs.turn,h,worlds);g.turn=actor;int category=responseCategory(g,played,axis);if(category<0)return {.5,.5,.5};
  auto shared=worlds.next();Rng r(shared);auto ranked=rankMicro(g,r,0,true);auto safe=safeActionsV9(g);ranked.erase(remove_if(ranked.begin(),ranked.end(),[&](auto q){return find(safe.begin(),safe.end(),q.a)==safe.end();}),ranked.end());
  for(int mode=0;mode<3;mode++){auto scores=responseOffsets(g,ranked,axis,mode);double total=0,matching=0,peak=scores.front().score;
   for(auto q:scores){int c=responseCategory(g,q.a,axis);if(c<0)continue;double weight=exp(max(-20.0,(q.score-peak)/8));total+=weight;if(c==category)matching+=weight;}
   likelihood[mode]+=(.05+.95*(total?matching/total:0))/4;
  }
 }return likelihood;
}
Act responseOpponentReply(const Game& g,Rng& rng,int style,int plan,const ResponseForecast* model){
 if(!model||!model->mask||!responseVoluntary(g,g.turn))return strategicOpponentReply(g,rng,style,plan);
 // All applicability gates are public, never conditional on actual concealed
 // human identities. Sampled worlds may of course contain hypothetical cards.
 vector<int> axes;if((model->mask&1)&&g.round==0&&!eliminationPressure(g))axes.push_back(0);if((model->mask&2)&&g.round==1&&g.p[g.turn].wins==1&&g.p[1-g.turn].wins==0)axes.push_back(1);if((model->mask&4)&&responseLoadedRow(g,g.turn)>=0&&responseLoadedRow(g,g.turn)<2)axes.push_back(2);if((model->mask&8)&&responseThreat(g,g.turn))axes.push_back(3);
 
 if(axes.empty())return strategicOpponentReply(g,rng,style,plan);
 Rng selection(rng.z^0xa215fe7391ULL);if(selection.u()<.6)return strategicOpponentReply(g,rng,style,plan);
 int axis=axes[selection.n(axes.size())];auto weights=model->weights[axis];double roll=selection.u();int mode=0;for(int k=0;k<3;k++)if((roll-=weights[k])<=0){mode=k;break;}
 if(!mode)return strategicOpponentReply(g,rng,style,plan);
 auto exact=catchUp(g,64);if(exact.complete)return exact.first;
 auto scores=responseOffsets(g,rankMicro(g,rng,style,true),axis,mode);auto safe=safeActionsV9(g);
 for(auto q:scores)if(find(safe.begin(),safe.end(),q.a)!=safe.end())return q.a;
 throw runtime_error("empty contextual response");
}

void writeResponse(ostream& out,const ResponseModel& m){out<<m.enabled<<' ';for(auto b:m.habits){out<<b.observations<<' ';for(double x:b.evidence)out<<x<<' ';}}
bool readResponse(istream& in,ResponseModel& m){ResponseModel parsed;int enabled=-1;if(!(in>>enabled)||(enabled!=0&&enabled!=1))return false;parsed.enabled=enabled;for(auto& b:parsed.habits){if(!(in>>b.observations)||b.observations<0||b.observations>100000)return false;for(double& x:b.evidence)if(!(in>>x)||!isfinite(x)||x< -8||x>0)return false;}m=parsed;return true;}
// Compare a specific counter's public effect, never the actual concealed hand.
bool responseOpportunity(const Game& before,int actor,int card){
 if(!responseCounter({0,card}))return false;
 auto g=observation(before,1-actor);g.p[actor].hand.clear();g.p[actor].deck.clear();
 int own=score(g,actor),enemy=score(g,1-actor),newPower=0;
 if(C[card].weather)g.weather.push_back({actor,card});
 else if(card==SCORCH)scorch(g);
 else {
  int copies=count_if(g.p[actor].board.begin(),g.p[actor].board.end(),[&](Unit u){return u.id==card;});
  g.p[actor].board.push_back({card,C[card].row});int flags=C[card].flags;
  if(flags&DRAGON)scorch(g,1-actor,0);if(flags&RSCORCH)scorch(g,1-actor,1);if(flags&SSCORCH)scorch(g,1-actor,2);if(flags&GSCORCH)scorch(g);
  if(count_if(g.p[actor].board.begin(),g.p[actor].board.end(),[&](Unit u){return u.id==card;})>copies){auto power=powers(g,actor);for(int i=int(power.size())-1;i>=0;i--)if(g.p[actor].board[i].id==card){newPower=power[i];break;}}
 }
 int loss=enemy-score(g,1-actor),collateral=max(0,own-(score(g,actor)-newPower));return loss>collateral;
}
struct ResponseWindow {int round=-1,drawn=-1,handAfter=-1;vector<int> waits;bool uncertain=false;};
void observeResponseCounter(const Game& before,const Game& after,Act played,int actor,ResponseBelief& belief,ResponseWindow& window){
 if(window.waits.size()!=C.size())window.waits.assign(C.size(),0);
 auto clear=[&]{fill(window.waits.begin(),window.waits.end(),0);};
 if(window.round!=before.round){clear();window.uncertain=window.round<0&&before.p[actor].drawn!=int(before.p[actor].hand.size());}
 else if(window.drawn!=before.p[actor].drawn||int(before.p[actor].hand.size())!=window.handAfter){clear();window.uncertain=true;}
 if(before.pending>=0||before.opening||played.kind==1||played.kind==2||(played.kind==0&&played.card==DEC)){clear();window.uncertain=true;}
 else if(responseCounter(played)&&responseOpportunity(before,actor,played.card)){
  // A forced final reveal can still witness earlier voluntary withholding.
  if(window.waits[played.card]>=2)belief.observe({.4,.1,.9});
  else if(responseVoluntary(before,actor)&&!window.uncertain)belief.observe({.4,.9,.1});
  clear();
 }else if(played.kind==0&&responseVoluntary(before,actor)){
  for(int c=0;c<int(C.size());c++)if(C[c].collectible&&(C[c].faction<0||C[c].faction==before.p[actor].faction)&&responseOpportunity(before,actor,c))window.waits[c]=min(100,window.waits[c]+1);
 }
 window.round=after.round;window.drawn=after.p[actor].drawn;window.handAfter=after.p[actor].hand.size();
 if(after.round!=before.round||after.pending>=0||after.p[actor].drawn!=before.p[actor].drawn||after.p[actor].hand.size()>before.p[actor].hand.size()){clear();window.uncertain=after.round==before.round;}
}

Act responseOpponentReply(const Game& g,Rng& rng,int style,int plan,const ResponseModel* model){ResponseForecast compiled(model);return responseOpponentReply(g,rng,style,plan,&compiled);}
