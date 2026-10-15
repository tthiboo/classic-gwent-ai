#pragma once
// Absolute state return learned from complete, observer-masked search games.
// Separate from rejected action-difference priors; never loaded in production.
vector<double> trajectoryFeatures(const Game& g,int side,int style){
 if(g.winner>=0||g.turn!=side||g.pending>=0||g.opening)
  throw runtime_error("trajectory value outside decision domain");
 auto x=resourceFeatures(observation(g,side),side,style);
 // Remove terminal outcome even though every training state is live.
 if(VALUE_FEATURES.at(59)!="terminal")throw runtime_error("trajectory terminal feature moved");
 x.erase(x.begin()+59);x.insert(x.begin(),1.0);return x;
}
uint64_t trajectorySchema(){
 uint64_t hash=resourceSchema();string name="GWENT_TRAJECTORY_VALUE_1:absolute:decision:intercept:remove-terminal59";
 for(unsigned char c:name){hash^=c;hash*=1099511628211ULL;}return hash;
}
struct TrajectoryValue {
 vector<double> weights;bool enabled=false;
 bool load(const string& path){
  enabled=false;weights.clear();ifstream in(path,ios::binary|ios::ate);
  if(!in||in.tellg()>65536)return false;in.seekg(0);
  string tag;uint64_t schema=0;int size=0;
  if(!(in>>tag>>schema>>size)||tag!="GWENT_TRAJECTORY_VALUE_1"||schema!=trajectorySchema()||size!=158)return false;
  vector<double> candidate(size);for(double& w:candidate)if(!(in>>w)||!isfinite(w)||abs(w)>10)return false;
  string extra;if(in>>extra)return false;weights=std::move(candidate);enabled=true;return true;
 }
 double predict(const Game& g,int side,int style)const{
  if(!enabled)throw runtime_error("disabled trajectory model");
  auto x=trajectoryFeatures(g,side,style);if(x.size()!=weights.size())throw runtime_error("trajectory schema mismatch");
  double v=.5;for(int i=0;i<(int)x.size();i++)v+=weights[i]*x[i];return clamp(v,0.0,1.0);
 }
};
