#include "human_scenarios.inc"
bool openingEvidenceEnabled=true;
bool pristineOpening(const Game& g){return g.steps==0&&g.round==0&&g.pending<0&&!g.opening&&!g.p[0].wins&&!g.p[1].wins&&!g.p[0].passed&&!g.p[1].passed&&g.p[0].board.empty()&&g.p[1].board.empty()&&g.weather.empty();}
// Conceding before seeing a single opposing play is irreversible. Ordinary
// rollout preference alone is insufficient: compare plausible nonpasses on
// fresh worlds, including opponents who exploit the round-one initiative.
void reviewOpeningPass(const Game& obs,const vector<Hypothesis>& h,uint64_t seed,
 int style,ReportV9& report,int maxTrials=24,int milliseconds=35){
 if(!openingEvidenceEnabled||report.chosen.kind!=3||!pristineOpening(obs))return;
 vector<EdgeV9> options;auto safe=safeActionsV9(obs);
 for(auto e:report.root)if(e.action.kind==0&&!C[e.action.card].kind&&find(safe.begin(),safe.end(),e.action)!=safe.end())options.push_back(e);
 if(options.empty())return;
 stable_sort(options.begin(),options.end(),[](auto a,auto b){double x=(a.value+.5)/(a.visits+1),y=(b.value+.5)/(b.visits+1);return x!=y?x>y:a.prior>b.prior;});
 vector<Act> rivals={options.front().action};
 for(auto e:options)if(!(e.action==rivals[0])&&rivals.size()<2)rivals.push_back(e.action);
 auto began=chrono::steady_clock::now();Rng worlds(seed^0x1ef482ab719ULL);int side=obs.turn;
 vector<double> sum(rivals.size()),squares(rivals.size());vector<array<double,3>> family(rivals.size());array<int,3> count{};
 for(int trial=0;trial<maxTrials;trial++){
  if(trial&&milliseconds>0&&chrono::duration<double,milli>(chrono::steady_clock::now()-began).count()>=milliseconds)break;
  auto world=sampleBelief(obs,side,h,worlds);auto common=worlds.next();world.rng=Rng(common);int axis=trial%3,persona=axis==0?0:axis==1?7:9;
  auto outcome=[&](Act a){auto g=world;Rng rng(common);apply(g,a);while(g.winner<0){if(g.steps>500)throw runtime_error("long opening review");apply(g,g.turn==side?fastV8(g,rng,style,64):humanScenarioReply(g,rng,axis,persona));}return g.winner==2?.5:g.winner==side?1.0:0.0;};
  double pass=outcome({3});for(int k=0;k<int(rivals.size());k++){double difference=outcome(rivals[k])-pass;sum[k]+=difference;squares[k]+=difference*difference;family[k][axis]+=difference;}count[axis]++;report.passTrials++;
 }
 int best=max_element(sum.begin(),sum.end())-sum.begin();double n=report.passTrials;report.passMargin=n?sum[best]/n:0;
 bool supported=n>=12;
 for(int k=0;k<int(rivals.size());k++){
  double mean=n?sum[k]/n:0,se=n>1?sqrt(max(0.0,(squares[k]-sum[k]*sum[k]/n)/(n-1))/n):1;
  supported&=mean<-(.06+se);
  for(int axis=0;axis<3;axis++)supported&=count[axis]>=4&&family[k][axis]<=0;
 }
 if(!supported){report.chosen=rivals[best];report.passOverridden=true;}
 report.passMilliseconds=chrono::duration<double,milli>(chrono::steady_clock::now()-began).count();
}
