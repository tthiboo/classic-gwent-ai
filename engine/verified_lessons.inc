// Local, verified preferences. No global fitted policy and no match-outcome
// labels. Only matching public/acting-hand situations receive bounded priors.
string lessonSketch(const Game& g,Act a){
 ostringstream s;s<<a.kind<<':'<<a.row<<':';
 auto card=[&](int c){if(c<0||c>=int(C.size())){s<<"-;";return;}auto x=C[c];s<<x.p<<','<<x.flags<<','<<x.kind<<','<<x.weather<<','<<x.row<<';';};
 card(a.card);if(a.kind==1)s<<g.p[g.turn].leader<<';';
 if(a.card==DEC&&a.target>=0&&a.target<int(g.p[g.turn].board.size())){auto u=g.p[g.turn].board[a.target];card(u.id);s<<u.row;}
 else if(a.kind==1&&a.target>=0)card(a.target);
 return s.str();
}
struct LessonQuery {string key;int orientation=0;};
LessonQuery lessonQuery(const Game& g,Act a,Act b){
 string x=lessonSketch(g,a),y=lessonSketch(g,b);if(x==y)return {};
 int orientation=x<y?1:-1;auto first=moveFeatures(g,orientation>0?a:b),second=moveFeatures(g,orientation>0?b:a);
 ostringstream key;key<<learningContext(g)<<'/'<<min(x,y)<<'/'<<max(x,y)<<'/'<<g.round<<':'<<weatherMask(g)<<':'<<g.p[g.turn].used<<':'<<g.p[1-g.turn].used<<':'<<(g.p[1-g.turn].known.size()==g.p[1-g.turn].hand.size())<<':';
 for(int k=0;k<MOVE_FEATURES;k++)key<<lround((first[k]-second[k])*8)<<',';
 return {key.str(),orientation};
}
struct LessonEvidence {int positions=0,contradictions=0;double sum=0;};
struct LessonCertificate {array<double,2> mean{},standardError{};array<double,6> family{};int trials=0;bool strongConfirmation=false;};
uint64_t verifiedLessonCatalog(){uint64_t h=1469598103934665603ULL;for(auto c:C){auto text=c.name+":"+to_string(c.p)+":"+to_string(c.flags)+":"+to_string(c.kind)+":"+to_string(c.weather)+":"+to_string(c.row)+":"+to_string(c.group)+":"+to_string(c.transform)+":"+to_string(c.faction)+":"+to_string(c.limit);for(unsigned char x:text){h^=x;h*=1099511628211ULL;}}return h;}
struct VerifiedLessons {
 map<string,LessonEvidence> cases;map<uint64_t,pair<string,int>> seen;string path;uint64_t generation=0;
 bool observe(const Game& g,Act a,Act b,uint64_t position,const LessonCertificate& proof){
  if(!position||g.pending>=0||g.opening||g.winner>=0||!proof.strongConfirmation||proof.trials<48)return false;
  for(int k=0;k<2;k++)if(!isfinite(proof.mean[k])||abs(proof.mean[k])>1||!isfinite(proof.standardError[k])||proof.standardError[k]<0||proof.standardError[k]>1||abs(proof.mean[k])<=.12+2*proof.standardError[k])return false;
  int sign=proof.mean[0]>0?1:-1;if(proof.mean[1]*sign<=0)return false;
  for(double x:proof.family)if(!isfinite(x)||x*sign<=0||abs(x)>1)return false;
  auto safe=safeActionsV9(g);if(find(safe.begin(),safe.end(),a)==safe.end()||find(safe.begin(),safe.end(),b)==safe.end())return false;
  auto q=lessonQuery(g,a,b);if(!q.orientation)return false;sign*=q.orientation;
  auto old=seen.find(position);if(old!=seen.end()){
   // Conflicting re-review quarantines the original case, irrespective of order.
   if(old->second!=make_pair(q.key,sign)){generation++;auto found=cases.find(old->second.first);if(found!=cases.end())found->second.contradictions=min(100000,found->second.contradictions+1);auto foundNew=cases.find(q.key);if(foundNew!=cases.end())foundNew->second.contradictions=min(100000,foundNew->second.contradictions+1);}
   return false;
  }
  if(!cases.count(q.key)&&cases.size()>=512)return false;
  auto& e=cases[q.key];if(e.positions>=128)return false;
  double mean=(proof.mean[0]+proof.mean[1])/2*q.orientation;
  if(e.positions&&e.sum*mean<0)e.contradictions=min(100000,e.contradictions+1);
  generation++;e.positions++;e.sum+=mean;seen[position]={q.key,sign};if(seen.size()>4096)seen.erase(seen.begin());return true;
 }
 vector<double> hints(const Game& g,const vector<Act>& acts,int* matches=nullptr)const{
  vector<double> result(acts.size());if(cases.empty()||g.pending>=0||g.opening||g.p[1-g.turn].passed)return result;
  // Most positions have no applicable context; avoid feature work in that case.
  string prefix=to_string(learningContext(g))+'/';auto it=cases.lower_bound(prefix);if(it==cases.end()||it->first.compare(0,prefix.size(),prefix)!=0)return result;
  for(int i=0;i<int(acts.size());i++)for(int j=i+1;j<int(acts.size());j++){
   auto q=lessonQuery(g,acts[i],acts[j]);if(!q.orientation)continue;auto found=cases.find(q.key);if(found==cases.end())continue;auto e=found->second;if(!e.positions||e.contradictions||abs(e.sum/e.positions)<=.12)continue;
   double weight=clamp(e.sum/e.positions*q.orientation*.35,-.35,.35);result[i]+=weight;result[j]-=weight;if(matches)(*matches)++;
  }
  for(double& x:result)x=clamp(x,-.35,.35);return result;
 }
 void write(ostream& out)const{out<<"VERIFIED_CASES_2 "<<verifiedLessonCatalog()<<' '<<cases.size()<<' '<<seen.size()<<' ';out.precision(17);map<string,int> indices;int index=0;for(auto p:cases){indices[p.first]=index++;out<<quoted(p.first)<<' '<<p.second.positions<<' '<<p.second.contradictions<<' '<<p.second.sum<<' ';}for(auto p:seen)out<<p.first<<' '<<indices.at(p.second.first)<<' '<<p.second.second<<' ';}
 bool read(istream& in){VerifiedLessons parsed;string schema;int n=-1,m=-1;uint64_t catalog=0;if(!(in>>schema>>catalog>>n>>m)||schema!="VERIFIED_CASES_2"||catalog!=verifiedLessonCatalog()||n<0||n>512||m<0||m>4096)return false;vector<string> keys;
  for(int i=0;i<n;i++){string key;LessonEvidence e;if(!(in>>quoted(key)>>e.positions>>e.contradictions>>e.sum)||key.empty()||key.size()>4096||e.positions<1||e.positions>128||e.contradictions<0||e.contradictions>100000||!isfinite(e.sum)||abs(e.sum)>e.positions||!parsed.cases.emplace(key,e).second)return false;keys.push_back(key);}
  for(int i=0;i<m;i++){uint64_t id;int index=-1,sign=0;if(!(in>>id>>index>>sign)||!id||index<0||index>=n||abs(sign)!=1||!parsed.seen.emplace(id,make_pair(keys[index],sign)).second)return false;}
  cases=std::move(parsed.cases);seen=std::move(parsed.seen);generation++;return true;
 }
 bool load(const string& file){path=file;ifstream in(file,ios::binary|ios::ate);if(!in||in.tellg()>2*1024*1024)return false;in.seekg(0);VerifiedLessons parsed;if(!parsed.read(in))return false;string extra;if(in>>extra)return false;cases=std::move(parsed.cases);seen=std::move(parsed.seen);generation++;return true;}
 bool save()const{if(path.empty())return false;ofstream out(path+".tmp");write(out);out<<'\n';out.close();return out&&!rename((path+".tmp").c_str(),path.c_str());}
 VerifiedLessons context(const Game& g)const{VerifiedLessons result;string prefix=to_string(learningContext(g))+'/';for(auto it=cases.lower_bound(prefix);it!=cases.end()&&it->first.compare(0,prefix.size(),prefix)==0;it++)result.cases.insert(*it);return result;}
} verifiedLessons;
struct ExactLessonSolver {
 int budget=100000,visited=0,ownSide=-1;unordered_map<string,Act> winning;chrono::steady_clock::time_point deadline=chrono::steady_clock::time_point::max();unordered_map<string,int> memo;
 int value(const Game& g){
  if(g.winner>=0)return g.winner==2?0:g.winner==0?1:-1;
  if(++visited>budget||g.steps>500||(visited%64==0&&chrono::steady_clock::now()>=deadline))throw Limit{};
  string key=stateKey(g);for(auto p:g.p){key+='D';for(int c:p.deck)key+=to_string(c)+',';}key+='R'+to_string(g.rng.z);
  auto it=memo.find(key);if(it!=memo.end())return it->second;int best=g.turn==0?-1:1;Act chosen;auto actions=g.turn==ownSide?safeActionsV9(g):legal(g);
  for(auto a:actions){auto after=g;apply(after,a);int v=value(after);if((g.turn==0&&v>=best)||(g.turn==1&&v<=best))chosen=a;best=g.turn==0?max(best,v):min(best,v);if(best==(g.turn==0?1:-1))break;}
  memo[key]=best;if(g.turn==ownSide&&(ownSide? -best:best)==1)winning[key]=chosen;return best;
 }
};
bool exactLessonDomain(const Game& g){if(g.pending>=0||g.opening||g.winner>=0||g.p[0].wins!=1||g.p[1].wins!=1||g.p[0].hand.size()+g.p[1].hand.size()>8)return false;for(auto p:g.p)if(p.leader==9&&!p.used)return false;return true;}

bool publicLessonEndgame(Game& g,bool allowPassed=false){
 int other=1-g.turn;if(!exactLessonDomain(g)||(!allowPassed&&g.p[other].passed)||g.p[other].known.size()!=g.p[other].hand.size())return false;
 for(auto p:g.p)if(!p.used&&p.leader!=4&&p.leader!=14&&p.leader!=15&&p.leader!=21)return false;
 g.p[other].hand=g.p[other].known;
 for(auto p:g.p){for(int c:p.hand)if(C[c].flags&(SPY|MED|MUSTER))return false;if(find(p.hand.begin(),p.hand.end(),DEC)!=p.hand.end())for(auto u:p.board)if(C[u.id].flags&(SPY|MED|MUSTER))return false;}
 return true;
}
struct VerifiedProofSession {const VerifiedLessons* owner=nullptr;uint64_t generation=0;unordered_map<string,Act> moves;};
array<VerifiedProofSession,2> verifiedProofSessions;
void clearVerifiedProofs(){for(auto& p:verifiedProofSessions)p={};}
string verifiedPublicKey(const Game& g){string key=stateKey(g);for(auto p:g.p){key+='D';for(int c:p.deck)key+=to_string(c)+',';}return key+'R'+to_string(g.rng.z);}
optional<Act> verifiedContinuation(const Game& actual,const VerifiedLessons* memory){
 if(!memory||memory->cases.empty())return {};auto g=observation(actual,actual.turn);if(!publicLessonEndgame(g,true))return {};g.p[0].deck.clear();g.p[1].deck.clear();auto& session=verifiedProofSessions[g.turn];
 if(session.owner!=memory||session.generation!=memory->generation)return {};auto found=session.moves.find(verifiedPublicKey(g));return found==session.moves.end()?optional<Act>{}:optional<Act>{found->second};
}
optional<Act> verifiedLessonProof(const Game& actual,Act incumbent,const VerifiedLessons* memory,int& matches){
 if(!memory||memory->cases.empty())return {};if(auto next=verifiedContinuation(actual,memory)){matches=1;return next;}
 auto g=observation(actual,actual.turn);int side=g.turn;if(!publicLessonEndgame(g))return {};
 auto acts=safeActionsV9(g);auto hints=memory->hints(g,acts,&matches);if(!matches)return {};
 vector<int> order(acts.size());iota(order.begin(),order.end(),0);stable_sort(order.begin(),order.end(),[&](int a,int b){return hints[a]>hints[b];});
 g.p[0].deck.clear();g.p[1].deck.clear();ExactLessonSolver solver;solver.ownSide=side;solver.budget=60000;solver.deadline=chrono::steady_clock::now()+chrono::milliseconds(35);
 try{
  auto adopt=[&](Act action){auto& session=verifiedProofSessions[side];session.owner=memory;session.generation=memory->generation;session.moves=std::move(solver.winning);session.moves[verifiedPublicKey(g)]=action;return optional<Act>{action};};
  if(find(acts.begin(),acts.end(),incumbent)!=acts.end()){auto after=g;apply(after,incumbent);if((side?-1:1)*solver.value(after)==1)return adopt(incumbent);}
  for(int i:order){if(acts[i]==incumbent)continue;auto after=g;apply(after,acts[i]);if((side?-1:1)*solver.value(after)!=1)continue;
   // Retain the entire proven winning continuation. Every opponent reply was
   // included; our later moves follow the certified safe policy, not a rollout.
   return adopt(acts[i]);
  }
 }catch(Limit&){}return {};
}
// Unsupported or incomplete proofs retain the mature policy unchanged. The
// learned preference controls which exact branch receives the bounded work.
int applyVerifiedLessonHints(vector<EdgeV9>&,const Game&,const VerifiedLessons*){return 0;}
