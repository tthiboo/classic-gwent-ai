// Concrete action preferences learned from fresh paired continuations. Features
// use the acting hand/multiset and public board, never concealed enemy cards.
// This is a bounded ranking hint, not a replacement for search or exact proofs.
constexpr int MOVE_BASE=24,MOVE_CONTEXT=13,MOVE_FEATURES=MOVE_BASE*MOVE_CONTEXT;
using MoveVector=array<double,MOVE_FEATURES>;
MoveVector moveFeatures(const Game& g,Act a){
 int s=g.turn,o=1-s;array<double,MOVE_BASE> b{};array<double,MOVE_CONTEXT> c{};
 b[0]=a.kind==3;b[1]=a.kind==1;
 int row=a.row,flags=0,power=0,weather=0,group=-1;
 if(a.card>=0&&a.card<(int)C.size()){
  const auto& card=C[a.card];flags=card.flags;power=card.p;weather=card.weather;group=card.group;
  if(row<0)row=card.row;b[2]=!card.kind;b[3]=power/15.0;
  for(int k=0;k<5;k++)b[4+k]=bool(flags&array<int,5>{HERO,SPY,MED,BOND,MUSTER}[k]);
  b[9]=a.card==DEC;b[10]=bool(weather);b[11]=a.card==CLEAR;
  b[12]=a.card==SCORCH||bool(flags&(DRAGON|RSCORCH|SSCORCH|GSCORCH));
  b[13]=a.card==HORN||bool(flags&UHORN);b[14]=bool(flags&MORALE);
  if(row>=0&&row<3)b[15+row]=1;
  auto partner=[&](int id){return id==a.card||(group>=0&&C[id].group==group);};
  if(flags&BOND){int n=0;for(int id:g.p[s].hand)n+=partner(id);for(auto u:g.p[s].board)n+=partner(u.id);b[18]=max(0,n-1)/4.0;}
  if(flags&MUSTER){int n=0;for(int id:g.p[s].hand)n+=partner(id);for(int id:g.p[s].deck)n+=partner(id);b[19]=max(0,n-1)/6.0;}
  if(flags&MED)b[20]=graveRecoveryValue(g,s)/80;
 }
 double ownMax=0,enemyMax=0;array<double,3> ownRows{},enemyRows{};
 for(int side:{s,o}){auto strengths=powers(g,side);for(int i=0;i<(int)strengths.size();i++)if(!(C[g.p[side].board[i].id].flags&HERO)){
  (side==s?ownRows:enemyRows)[g.p[side].board[i].row]+=strengths[i];
  auto& maximum=side==s?ownMax:enemyMax;maximum=max(maximum,double(strengths[i]));
 }}
 if(weather)for(int r=0;r<3;r++)if(weather&(1<<r))b[21]+=(enemyRows[r]-ownRows[r])/60;
 if(b[12])b[22]=(enemyMax-ownMax)/30;
 if(b[13]&&row>=0&&row<3)b[23]=ownRows[row]/40;
 c={1.0,(score(g,s)-score(g,o))/60.0,(int(g.p[s].hand.size())-int(g.p[o].hand.size()))/12.0,
 double(g.round==0),double(g.p[s].wins>0),double(g.p[o].wins>0),double(g.p[o].passed),
 double(g.p[s].faction==0),double(g.p[s].faction==1),double(g.p[s].faction==2),double(g.p[s].faction==3),double(g.p[s].faction==4),double(row>=0&&row<3&&bool(weatherMask(g)&(1<<row)))};
 MoveVector x{};for(int i=0;i<MOVE_BASE;i++)for(int j=0;j<MOVE_CONTEXT;j++)x[i*MOVE_CONTEXT+j]=clamp(b[i],-2.0,2.0)*clamp(c[j],-2.0,2.0);
 return x;
}
struct MoveLearning {
 MoveVector weights{};array<int,9> categoryLessons{};int lessons=0,matchLessons=0;uint64_t match=0;string path;vector<uint64_t> seen;
 void beginMatch(uint64_t seed){match=seed;matchLessons=0;}
 double prediction(const Game& g,Act a)const{auto x=moveFeatures(g,a);double sum=0;for(int k=0;k<MOVE_FEATURES;k++)sum+=weights[k]*x[k];return clamp(sum,-4.0,4.0);}
 double multiplier(const Game& g,Act a)const{return lessons>=20&&categoryLessons[decisionClass(a)]>=12?exp(clamp(prediction(g,a)/4,-.3,.3)):1;}
 vector<double> adjustments(const Game& g,const vector<Act>& actions)const{
  vector<double> result(actions.size());if(lessons<20||actions.empty())return result;
  array<double,9> sums{};array<int,9> counts{};
  for(int i=0;i<(int)actions.size();i++){int k=decisionClass(actions[i]);if(categoryLessons[k]>=12){result[i]=prediction(g,actions[i]);sums[k]+=result[i];counts[k]++;}}
  for(int i=0;i<(int)actions.size();i++){int k=decisionClass(actions[i]);result[i]=counts[k]>1?clamp(result[i]-sums[k]/counts[k],-.3,.3):0;}return result;
 }
 bool observe(const Game& g,Act a,Act b,double mean,double standardError,int trials){
  // One position contributes ONE lesson, not its correlated validation trials.
  if(lessons>=1000000||a==b||decisionClass(a)!=decisionClass(b)||trials<6||trials>128||!isfinite(mean)||abs(mean)>1||!isfinite(standardError)||standardError<0||standardError>1||abs(mean)<=.04+standardError||matchLessons>=12)return false;
  string ak=actionKey(a),bk=actionKey(b);string key=to_string(match)+'#'+informationKeyV9(g,g.turn)+'#'+min(ak,bk)+'#'+max(ak,bk);
  uint64_t hash=1469598103934665603ULL;for(unsigned char c:key){hash^=c;hash*=1099511628211ULL;}if(find(seen.begin(),seen.end(),hash)!=seen.end())return false;
  auto x=moveFeatures(g,a),y=moveFeatures(g,b);double norm=0,predicted=0;
  for(int k=0;k<MOVE_FEATURES;k++){x[k]-=y[k];norm+=x[k]*x[k];predicted+=weights[k]*x[k];}if(norm<1e-10)return false;
  double rate=.25/(1+sqrt(double(lessons))/30),error=clamp(predicted,-1.0,1.0)-mean;
  for(int k=0;k<MOVE_FEATURES;k++)weights[k]=clamp(.999*weights[k]-rate*error*x[k]/max(1.0,norm),-.5,.5);
  lessons++;categoryLessons[decisionClass(a)]++;matchLessons++;seen.push_back(hash);if(seen.size()>4096)seen.erase(seen.begin());return true;
 }
 bool load(const string& file){path=file;ifstream in(file);string schema;int count=-1,n=-1;
  if(!(in>>schema>>count>>n)||schema!="GWENT_MOVE_2"||count<0||count>1000000||n<0||n>4096)return false;
  array<int,9> categories{};for(int& x:categories)if(!(in>>x)||x<0||x>count)return false;if(accumulate(categories.begin(),categories.end(),0)!=count)return false;
  MoveVector parsed{};vector<uint64_t> ids(n);for(double& w:parsed)if(!(in>>w)||!isfinite(w)||abs(w)>.5)return false;
  for(auto& id:ids)if(!(in>>id))return false;string extra;if(in>>extra)return false;
  weights=parsed;lessons=count;categoryLessons=categories;seen=std::move(ids);return true;
 }
 void save()const{if(path.empty())return;ofstream out(path+".tmp");out.precision(17);out<<"GWENT_MOVE_2 "<<lessons<<' '<<seen.size()<<'\n';for(int n:categoryLessons)out<<n<<' ';out<<'\n';for(double w:weights)out<<w<<' ';out<<'\n';for(auto id:seen)out<<id<<' ';out.close();if(out)rename((path+".tmp").c_str(),path.c_str());}
} moveLearning;
// Preserve the existing allocation BETWEEN categories; lessons only refine
// choices WITHIN a supported category. A unit-pair lesson cannot boost passing
// or weather merely because their raw feature values use a different offset.
void applyMoveHints(vector<EdgeV9>& edges,const vector<double>& hints){
 if(none_of(hints.begin(),hints.end(),[](double x){return x!=0;}))return;
 array<double,9> before{},after{};for(int i=0;i<(int)edges.size();i++){int k=decisionClass(edges[i].action);before[k]+=edges[i].prior;after[k]+=edges[i].prior*exp(hints[i]);}
 for(int i=0;i<(int)edges.size();i++){int k=decisionClass(edges[i].action);if(after[k]>0)edges[i].prior*=exp(hints[i])*before[k]/after[k];}
}
