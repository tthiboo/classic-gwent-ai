#pragma once
// Offline categorical actor. Search evidence trains a nonlinear choice policy;
// terminal game outcomes are not action labels. No production loader/global.
constexpr int ACTOR_FEATURES=530,ACTOR_HIDDEN=32;
using ActorVector=array<double,ACTOR_FEATURES>;
ActorVector actorFeaturesMasked(const Game& g,Act a,int style,const vector<double>& state){
 int s=g.turn,o=1-s;
 if(style<0||style>2||g.winner>=0||g.pending>=0||g.opening)throw runtime_error("actor outside normal decision domain");
 ActorVector x{};auto base=moveFeatures(g,a);copy(base.begin(),base.end(),x.begin());int k=MOVE_FEATURES;
 for(int l=0;l<22;l++)x[k++]=a.kind==1&&g.p[s].leader==l;
 int target=-1,row=-1;
 if(a.kind==1&&a.target>=0)target=a.target;
 if(a.kind==0&&a.card==DEC){if(a.target<0||a.target>=int(g.p[s].board.size()))throw runtime_error("invalid actor decoy target");auto u=g.p[s].board[a.target];target=u.id;row=u.row;}
 if(target>=0){if(target>=int(C.size()))throw runtime_error("invalid actor card target");auto c=C[target];if(row<0)row=c.row;
  x[k]=c.p/15.0;for(int f=0;f<15;f++)x[k+1+f]=bool(c.flags&(1<<f));
  for(int r=0;r<3;r++)x[k+16+r]=row==r;x[k+19]=clamp(quality(target)/30.0,-2.0,2.0);
 }k+=20;
 for(int j=0;j<3;j++)x[k++]=style==j;
 for(int j=0;j<5;j++)x[k++]=g.p[o].faction==j;
 x[k++]=g.p[s].hand.size()/15.0;x[k++]=g.p[s].deck.size()/30.0;
 x[k++]=g.p[s].grave.size()/30.0;x[k++]=g.p[o].grave.size()/30.0;
 x[k++]=graveRecoveryValue(g,s)/100.0;x[k++]=graveRecoveryValue(g,o)/100.0;
 x[k++]=g.round/2.0;x[k++]=g.p[s].drawn/30.0;
 x[k++]=!g.p[s].used;x[k++]=!g.p[o].used;
 if(state.size()!=158)throw runtime_error("invalid actor shared state");for(double v:state)x[k++]=v;
 if(k!=ACTOR_FEATURES)throw runtime_error("actor feature dimensions moved");
 for(double& v:x){if(!isfinite(v))throw runtime_error("nonfinite actor feature");v=clamp(v,-4.0,4.0);}return x;
}
ActorVector actorFeatures(const Game& actual,Act a,int style){
 auto g=observation(actual,actual.turn);auto state=trajectoryFeatures(g,g.turn,style);return actorFeaturesMasked(g,a,style,state);
}
uint64_t actorSchema(){uint64_t h=resourceSchema();string name="GWENT_SEARCH_ACTOR_2:530:32:tanh:categorical-residual:tactical-offset:move312:leader22:target20:style3:enemy5:resources10:state158";for(unsigned char c:name){h^=c;h*=1099511628211ULL;}return h;}
struct SearchActor {
 array<double,ACTOR_FEATURES*ACTOR_HIDDEN> input{};
 array<double,ACTOR_HIDDEN> bias{},output{};bool enabled=false;
 bool hasResidual()const{return any_of(output.begin(),output.end(),[](double value){return value!=0;});}
 bool load(const string& path){
  enabled=false;ifstream in(path,ios::binary|ios::ate);if(!in||in.tellg()>1048576)return false;in.seekg(0);
  string tag;uint64_t schema;int features,hidden;
  if(!(in>>tag>>schema>>features>>hidden)||tag!="GWENT_SEARCH_ACTOR_2"||schema!=actorSchema()||features!=ACTOR_FEATURES||hidden!=ACTOR_HIDDEN)return false;
  auto w=input;auto b=bias,v=output;for(double& a:w)if(!(in>>a)||!isfinite(a)||abs(a)>20)return false;
  for(double& a:b)if(!(in>>a)||!isfinite(a)||abs(a)>20)return false;
  for(double& a:v)if(!(in>>a)||!isfinite(a)||abs(a)>20)return false;
  string extra;if(in>>extra)return false;input=w;bias=b;output=v;enabled=true;return true;
 }
 double logit(const ActorVector& x)const{
  if(!enabled)throw runtime_error("disabled search actor");double value=0;
  for(int j=0;j<ACTOR_HIDDEN;j++){double z=bias[j];for(int i=0;i<ACTOR_FEATURES;i++)z+=input[j*ACTOR_FEATURES+i]*x[i];value+=output[j]*tanh(z);}return value;
 }
 vector<double> probabilities(const Game& g,const vector<Act>& moves,int style,const vector<double>& tactical={})const{
  if(!tactical.empty()&&tactical.size()!=moves.size())throw runtime_error("invalid actor baseline dimensions");
  if(moves.empty())throw runtime_error("empty actor policy");auto observed=observation(g,g.turn);auto state=trajectoryFeatures(observed,g.turn,style);vector<double> p;double maximum=-1e100,sum=0;
  for(int i=0;i<int(moves.size());i++){
   if(!tactical.empty()&&(!isfinite(tactical[i])||tactical[i]<=0||tactical[i]>1))throw runtime_error("invalid actor baseline probability");
   p.push_back(logit(actorFeaturesMasked(observed,moves[i],style,state))+(tactical.empty()?0:log(tactical[i])));maximum=max(maximum,p.back());
  }
  for(double& v:p){v=exp(v-maximum);sum+=v;}for(double& v:p)v/=sum;return p;
 }
};
