#pragma once
#include "resource_value.inc"
// Frozen offline candidate, opt-in only. Qualified label agreement does NOT
// authorize deployment. Uses the inexpensive action representation selected by
// development; the richer after-state representation was not selected.
struct PairValue {
 MoveVector weights{};bool enabled=false;
 bool load(const string& file){enabled=false;ifstream in(file,ios::binary|ios::ate);if(!in||in.tellg()>65536)return false;in.seekg(0);
  string schema;uint64_t fingerprint=0;int n=0;if(!(in>>schema>>fingerprint>>n)||schema!="GWENT_PAIR_1"||fingerprint!=resourceSchema()||n!=MOVE_FEATURES)return false;
  MoveVector parsed{};for(double& w:parsed)if(!(in>>w)||!isfinite(w)||abs(w)>10)return false;
  string extra;if(in>>extra)return false;weights=parsed;enabled=true;return true;
 }
 double prediction(const Game& g,Act a)const{if(!enabled)return 0;auto x=moveFeatures(g,a);double sum=0;for(int j=0;j<MOVE_FEATURES;j++)sum+=weights[j]*x[j];return sum;}
} pairValue;
// Blend rankings, not simulated outcome estimates. Root choice, exact proofs,
// safety filters and rollout outcomes remain unchanged. Three quarters of the
// original tactical allocation remain; coarse learned multipliers are retained.
bool applyPairPriors(vector<EdgeV9>& edges,const Game& observed,bool coarseRoot=false){
 if(!pairValue.enabled||edges.size()<2)return false;int n=edges.size();vector<double> scores(n);vector<int> learned(n);iota(learned.begin(),learned.end(),0);
 for(int i=0;i<n;i++)scores[i]=pairValue.prediction(observed,edges[i].action);
 auto bounds=minmax_element(scores.begin(),scores.end());if(*bounds.second-*bounds.first<1e-9)return false;
 // Ties retain the original priority. Root category multipliers apply to both
 // halves of the blend; future nodes retain their original unweighted scheme.
 stable_sort(learned.begin(),learned.end(),[&](int a,int b){return scores[a]!=scores[b]?scores[a]>scores[b]:edges[a].prior>edges[b].prior;});
 vector<int> ranks(n);for(int i=0;i<n;i++)ranks[learned[i]]=i;
 for(int i=0;i<n;i++){double tactical=edges[i].prior;
  double coarse=coarseRoot?decisionLearning.multiplier(learningContext(observed),decisionClass(edges[i].action)):1;
  edges[i].prior=.75*tactical+.25*(.12/n+.88*exp(-.55*ranks[i]))*coarse;
 }return true;
}
