// A small contextual prior, trained on PAIRED counterfactual simulations.
// It affects search allocation only, never legality, safety filters or exact proofs.
int decisionClass(Act a) {
 if(a.kind==3)return 0;if(a.kind==1)return 1;if(a.card<0)return 2;
 const auto& c=C[a.card];
 if(c.flags&SPY||a.card==DEC)return 3;if(c.flags&MED)return 4;
 if(c.weather||a.card==CLEAR)return 5;
 if(a.card==SCORCH||c.flags&(DRAGON|RSCORCH|SSCORCH|GSCORCH))return 6;
 if(a.card==HORN||c.flags&(UHORN|MORALE)||a.card==MARD)return 7;
 return 8;
}
int learningContext(const Game& g){
 int s=g.turn,o=1-s,d=score(g,s)-score(g,o),cards=int(g.p[s].hand.size())-int(g.p[o].hand.size());
 // Match point is distinct from a disposable round. No hidden card identities.
 int phase=g.p[o].wins?2:g.p[s].wins?1:0;
 int lead=d<-5?0:d>5?2:1,adv=cards<0?0:cards>0?2:1;
 return (((g.p[s].faction*3+phase)*3+lead)*3+adv)*2+g.p[o].passed;
}
struct PairedEvidence {int count=0;double sum=0;};
struct DecisionLearning {
 map<int,PairedEvidence> pairs;string path;
 static int key(int context,int a,int b){return context*81+min(a,b)*9+max(a,b);}
 void observe(int context,int a,int b,double difference){
  if(context<0||context>=270||a<0||a>8||b<0||b>8||a==b||!isfinite(difference)||abs(difference)>1)return;
  int k=key(context,a,b);if(!pairs.count(k)&&pairs.size()>=2048)return;
  auto& p=pairs[k];if(p.count>=4096)return;
  p.count++;p.sum+=a<b?difference:-difference;
 }
 double multiplier(int context,int category)const {
  double adjustment=0;int supported=0;
  for(int other=0;other<9;other++)if(other!=category){
   auto it=pairs.find(key(context,category,other));if(it==pairs.end())continue;auto p=it->second;
   if(p.count<32)continue;
   // Conservative evidence threshold, not a guarantee about human win rates:
   // contexts are aggregated and rollouts use approximate policies/beliefs.
   double mean=p.sum/p.count*(category<other?1:-1),radius=sqrt(2*log(40.0)/p.count);
   if(abs(mean)<=radius)continue;
   adjustment+=copysign(min(.3,abs(mean)-radius),mean);supported++;
  }
  return supported?exp(adjustment/supported):1;
 }
 void load(string file){path=file;pairs.clear();ifstream in(path);string version;int size;
  if(!(in>>version>>size)||version!="GWENT_PRIOR_1"||size<0||size>2048)return;
  map<int,PairedEvidence> parsed;for(int i=0;i<size;i++){int k;PairedEvidence p;
   if(!(in>>k>>p.count>>p.sum)||k<0||k>=270*81||p.count<1||p.count>4096||!isfinite(p.sum)||abs(p.sum)>p.count)return;
   parsed[k]=p;
  }pairs=std::move(parsed);
 }
 void save()const {if(path.empty())return;ofstream out(path+".tmp");out<<"GWENT_PRIOR_1 "<<pairs.size()<<'\n';
  for(auto [k,p]:pairs)out<<k<<' '<<p.count<<' '<<p.sum<<'\n';out.close();if(out)rename((path+".tmp").c_str(),path.c_str());}
} decisionLearning;
