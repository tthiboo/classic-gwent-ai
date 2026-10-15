// Versioned, self-contained observer input for later offline reanalysis.
// The actual enemy hand/draw order and the live RNG state are never encoded.
struct ReplaySnapshot {DecisionLearning learning;Game observed;Deck own;vector<Deck> library;uint64_t seed=0;int style=0;array<double,3> weights{1,1,1},plans{1,0,0};bool matchPlans=false,lockedPlans=false,characterPlans=false;ResponseModel response;VerifiedLessons lessons;optional<Act> provenLesson;};
uint64_t replayCatalog(){uint64_t hash=1469598103934665603ULL;for(auto c:C){string s=c.name+":"+to_string(c.p)+":"+to_string(c.flags)+":"+to_string(c.row);for(unsigned char b:s){hash^=b;hash*=1099511628211ULL;}}return hash;}
string encodeReplay(const Game& actual,int viewer,const Deck& own,const vector<Deck>& library,uint64_t seed,int style,array<double,3> weights,array<double,3> plans={1,0,0},bool matchPlans=false,bool lockedPlans=false,bool characterPlans=false,const ResponseModel* response=nullptr,const VerifiedLessons* lessons=nullptr){
 if(viewer!=actual.turn)throw runtime_error("replay viewer must be the acting player");
 auto g=observation(actual,viewer);ostringstream out;out.precision(17);out<<"GWENT_REPLAY_1 "<<replayCatalog()<<' '<<seed<<' '<<style<<' ';for(double w:weights)out<<w<<' ';
 int context=learningContext(g),evidence=0;for(auto item:decisionLearning.pairs)evidence+=item.first/81==context;out<<evidence<<' ';for(auto item:decisionLearning.pairs)if(item.first/81==context)out<<item.first<<' '<<item.second.count<<' '<<item.second.sum<<' ';
 auto vectorOut=[&](const vector<int>& v){out<<v.size()<<' ';for(int n:v)out<<n<<' ';};
 auto deckOut=[&](const Deck& d){out<<d.faction<<' '<<d.leader<<' ';vectorOut(d.cards);};
 deckOut(own);out<<library.size()<<' ';for(auto d:library)deckOut(d);
 out<<g.turn<<' '<<g.pending<<' '<<g.round<<' '<<g.winner<<' '<<g.steps<<' '<<g.emhyrSpecial<<' '<<g.heroThreshold<<' '<<g.pendingKind<<' '<<g.remaining<<' '<<g.openingTurn<<' '<<g.opening<<' ';
 for(auto p:g.p){out<<p.faction<<' '<<p.leader<<' '<<p.wins<<' '<<p.lhorn<<' '<<p.used<<' '<<p.passed<<' '<<p.drawn<<' '<<p.spies<<' '<<p.revives<<' ';vectorOut(p.hand);vectorOut(p.deck);vectorOut(p.grave);vectorOut(p.known);out<<p.board.size()<<' ';for(auto u:p.board)out<<u.id<<' '<<u.row<<' ';}
 out<<g.weather.size()<<' ';for(auto x:g.weather)out<<x.first<<' '<<x.second<<' ';
 out<<g.openingQueue.size()<<' ';for(auto x:g.openingQueue)out<<x.first<<' '<<x.second<<' ';
 out<<g.changes.size()<<' ';for(auto x:g.changes)out<<x.first<<' '<<x.second<<' ';
 // Old neutral snapshots keep their exact encoding. Learned forecasts are
 // explicit inputs: replay must never silently use the current live memory.
 if(plans!=array<double,3>{1,0,0}){double total=0;for(double x:plans){if(!isfinite(x)||x<0)throw runtime_error("invalid replay plan");total+=x;}if(!isfinite(total)||total<=0)throw runtime_error("empty replay plan");out<<"ROUND_PLAN_WEIGHTS_1 ";for(double x:plans)out<<x<<' ';}
 if(lockedPlans&&!matchPlans)throw runtime_error("locked replay requires plans");
 if(matchPlans)out<<"MATCH_PLAN_1 "<<matchPlans<<' '<<lockedPlans<<' ';
 if(characterPlans)out<<"CHARACTER_SEARCH_1 ";
 if(response&&response->enabled){out<<"CONTEXT_RESPONSE_1 ";writeResponse(out,*response);}
 if(lessons&&!lessons->cases.empty()){auto scoped=lessons->context(g);if(!scoped.cases.empty()){out<<"VERIFIED_LESSONS_1 ";scoped.write(out);}}
 if(auto action=verifiedContinuation(actual,lessons))out<<"PROVEN_LESSON_1 "<<action->kind<<' '<<action->card<<' '<<action->target<<' '<<action->row<<' ';
 return out.str();
}
ReplaySnapshot decodeReplay(const string& text){
 if(text.size()>1000000)throw runtime_error("oversized replay");istringstream in(text);ReplaySnapshot r;string version;uint64_t catalog=0;
 in>>version>>catalog>>r.seed>>r.style;if(version!="GWENT_REPLAY_1"||catalog!=replayCatalog())throw runtime_error("incompatible replay");
 for(double& w:r.weights){in>>w;if(!isfinite(w)||w<0)throw runtime_error("invalid replay weight");}
 auto count=[&](){int n=-1;in>>n;if(!in||n<0||n>1000)throw runtime_error("invalid replay count");return n;};
 int evidence=count();for(int i=0;i<evidence;i++){int k=-1;PairedEvidence p;in>>k>>p.count>>p.sum;if(!in||k<0||k>=270*81||p.count<1||p.count>4096||!isfinite(p.sum)||abs(p.sum)>p.count)throw runtime_error("invalid replay learning");r.learning.pairs[k]=p;}
 auto vectorIn=[&](vector<int>& v){int n=count();v.resize(n);for(int& c:v){in>>c;if(!in||c< -1||c>=(int)C.size())throw runtime_error("invalid replay card");}};
 auto deckIn=[&](Deck& d){in>>d.faction>>d.leader;vectorIn(d.cards);d.name="replay";};
 deckIn(r.own);r.library.resize(count());for(auto& d:r.library)deckIn(d);
 auto& g=r.observed;in>>g.turn>>g.pending>>g.round>>g.winner>>g.steps>>g.emhyrSpecial>>g.heroThreshold>>g.pendingKind>>g.remaining>>g.openingTurn>>g.opening;
 for(auto& p:g.p){in>>p.faction>>p.leader>>p.wins>>p.lhorn>>p.used>>p.passed>>p.drawn>>p.spies>>p.revives;vectorIn(p.hand);vectorIn(p.deck);vectorIn(p.grave);vectorIn(p.known);p.board.resize(count());for(auto& u:p.board){in>>u.id>>u.row;if(u.id<0||u.id>=(int)C.size()||u.row<0||u.row>2)throw runtime_error("invalid replay unit");}}
 g.weather.resize(count());for(auto& x:g.weather)in>>x.first>>x.second;
 g.openingQueue.resize(count());for(auto& x:g.openingQueue)in>>x.first>>x.second;
 int changes=count();for(int i=0;i<changes;i++){int c=0,n=0;in>>c>>n;if(!in||c<0||c>=(int)C.size()||abs(double(n))>1000)throw runtime_error("invalid replay change");g.changes[c]=n;}
 if(!in||g.turn<0||g.turn>1||r.style<0||r.style>2)throw runtime_error("invalid replay state");string extra;
 bool roundSeen=false,matchSeen=false,lessonsSeen=false;
 while(in>>extra){
  if(extra=="ROUND_PLAN_WEIGHTS_1"&&!roundSeen){roundSeen=true;double total=0;for(double& x:r.plans){if(!(in>>x)||!isfinite(x)||x<0)throw runtime_error("invalid replay plan");total+=x;}if(!isfinite(total)||total<=0)throw runtime_error("empty replay plan");}
  else if(extra=="MATCH_PLAN_1"&&!matchSeen){matchSeen=true;int enabled=-1,locked=-1;if(!(in>>enabled>>locked)||enabled!=1||(locked!=0&&locked!=1))throw runtime_error("invalid replay match plan");r.matchPlans=true;r.lockedPlans=locked;}
  else if(extra=="CHARACTER_SEARCH_1"&&!r.characterPlans)r.characterPlans=true;
  else if(extra=="CONTEXT_RESPONSE_1"&&!r.response.enabled){if(!readResponse(in,r.response)||!r.response.enabled)throw runtime_error("invalid replay response model");}
  else if(extra=="VERIFIED_LESSONS_1"&&!lessonsSeen){lessonsSeen=true;if(!r.lessons.read(in)||r.lessons.cases.empty()||!r.lessons.seen.empty())throw runtime_error("invalid replay lessons");}
  else if(extra=="PROVEN_LESSON_1"&&!r.provenLesson){Act a;if(!(in>>a.kind>>a.card>>a.target>>a.row))throw runtime_error("invalid proven continuation");r.provenLesson=a;}
  else throw runtime_error("trailing replay data");
 }
 auto validCard=[&](int c){return c>=0&&c<(int)C.size();};
 auto checkDeck=[&](const Deck& d){if(d.faction<0||d.faction>4||d.leader<0||d.leader>21||!all_of(d.cards.begin(),d.cards.end(),validCard))throw runtime_error("invalid replay roster");};
 checkDeck(r.own);for(auto d:r.library)checkDeck(d);
 if(g.pending< -1||g.pending>1||g.round<0||g.round>3||g.steps<0||g.steps>500)throw runtime_error("invalid replay phase");
 for(int s=0;s<2;s++){auto p=g.p[s];if(p.faction<0||p.faction>4||p.leader<0||p.leader>21)throw runtime_error("invalid replay player");
  for(auto v:{p.grave,p.known})if(!all_of(v.begin(),v.end(),validCard))throw runtime_error("invalid public replay card");
  for(auto v:{p.hand,p.deck})for(int c:v)if(s==g.turn?!validCard(c):c!=-1)throw runtime_error("invalid replay information boundary");
 }
 for(auto list:{g.weather,g.openingQueue})for(auto item:list)if(item.first<0||item.first>1||!validCard(item.second))throw runtime_error("invalid replay public effect");
 g.rng=Rng(0);if(r.provenLesson){auto publicGame=g;if(!publicLessonEndgame(publicGame,true))throw runtime_error("nonpublic proven continuation");publicGame.p[0].deck.clear();publicGame.p[1].deck.clear();auto safe=safeActionsV9(publicGame);if(find(safe.begin(),safe.end(),*r.provenLesson)==safe.end())throw runtime_error("unsafe proven continuation");apply(publicGame,*r.provenLesson);ExactLessonSolver solver;solver.ownSide=g.turn;try{if((g.turn?-1:1)*solver.value(publicGame)!=1)throw runtime_error("unproven lesson continuation");}catch(Limit&){throw runtime_error("incomplete lesson continuation");}}return r;
}
