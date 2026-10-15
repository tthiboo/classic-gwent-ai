#include "bot_v9.cpp"
#include "classic.inc"
constexpr const char* ENGINE_BUILD="20261015-opening-render-1";
string quote(const string& x){ostringstream s;s<<'"';for(unsigned char c:x){if(c=='"'||c=='\\')s<<'\\'<<c;else if(c=='\n')s<<"\\n";else if(c<32)s<<" ";else s<<c;}s<<'"';return s.str();}
vector<Deck> originals;
Deck findDeck(string name){for(auto d:originals)if(d.name==name)return d;throw runtime_error("Missing deck");}
const array<vector<int>,5> factionLeaders={vector<int>{0,1,2,6,7},vector<int>{3,4,5,8,9},vector<int>{10,11,12,13,14},vector<int>{15,16,17,18,19},vector<int>{20,21}};
Deck preset(int faction,int leader){
 Deck d;d.name="classic_"+to_string(faction);d.faction=faction;d.leader=leader;
 auto put=[&](string n,int copies=1){for(int i=0;i<copies;i++)d.cards.push_back(id(n));};
 if(faction<2){d=findDeck(faction==0?"N_siege_F0S2C1":"G_impera_C1");d.leader=leader;if(faction==1){erase1(d.cards,SCORCH);put("Impenetrable Fog");put("Torrential Rain");}return d;}
 for(string n:{"Geralt of Rivia","Cirilla Fiona Elen Riannon","Yennefer of Vengerberg","Mysterious Elf (Avallach)","Villentretenmerth"})put(n);
 if(faction==2){for(string n:{"Draug","Imlerith","Leshen","Kayran","Vampire: Bruxa","Vampire: Ekimmara","Vampire: Fleder","Vampire: Garkain","Vampire: Katakan","Crone: Brewess","Crone: Weavess","Crone: Whispess","Arachas Behemoth","Cow"})put(n);put("Arachas",3);put("Commander's Horn",2);}
 if(faction==3){for(string n:{"Eithne","Iorveth","Isengrim Faoiltiarna","Saesenthessis","Milva","Schirru","Yaevinn","Barclay Els"})put(n);put("Havekar Healer",3);put("Havekar Smuggler",3);put("Dwarven Skirmisher",3);put("Commander's Horn");}
 if(faction==4){for(string n:{"Cerys","Ermion","Hjalmar","Berserker","Birna Bran","Olaf","Kambi","Clan Dimun Pirate"})put(n);put("War Longship",3);put("Clan Drummond Shield Maiden",3);put("Young Berserker",3);put("Mardroeme",2);put("Skellige Storm");put("Commander's Horn");}
 put("Decoy",3);put("Scorch",faction==4?1:2);put("Clear Weather");if(faction!=4)put("Biting Frost");return d;
}
VerifiedLessons starterLessons,activeVerifiedLessons;
Game game;array<Deck,2> decks;vector<Deck> library;int swaps=0;bool begun=false,ready=false,strong=true;vector<int> returned;vector<string> journal;Rng decisions(1);
#include "card_skins.inc"
int lastSkin=-1;
void logLine(string s){journal.push_back(s);if(journal.size()>100)journal.erase(journal.begin());}
void validateDeck(const Deck& d){int units=0,specials=0;map<int,int> counts;for(int c:d.cards){if(c<0||c>=(int)C.size()||C[c].collectible==0||(C[c].faction>=0&&C[c].faction!=d.faction))throw runtime_error("В колоде недоступная карта");if(++counts[c]>C[c].limit)throw runtime_error("Слишком много копий карты");units+=!C[c].kind;specials+=!!C[c].kind;}if(units<22||specials>10)throw runtime_error("Нужно минимум 22 бойца и не более 10 спецкарт");}
#include "preparation.inc"
#include "adaptive.inc"
#include "replay_snapshot.inc"
#include "real_experience.inc"
string pendingReplay;
int pendingReplayStep=-1;
// Bounded local decision records for offline review; never serialize the human hand.
string traceMatch;
void recordDecisionV9(const Game& before,Act action) {
 if(memory.path.empty())return;
 string path=memory.path+".decisions.jsonl";
 ifstream sizeCheck(path,ios::binary|ios::ate);
 if(sizeCheck&&sizeCheck.tellg()>4*1024*1024){sizeCheck.close();rename(path.c_str(),(path+".previous").c_str());}
 ofstream out(path,ios::app);if(!out)return;
 out<<"{\"version\":12,\"match\":"<<quote(traceMatch)<<",\"actor\":"<<before.turn
 <<",\"public\":"<<quote(publicKey(before))<<",\"action\":"<<quote(actionKey(action))
 <<",\"winnerAfter\":"<<game.winner;
 if(before.turn==1){
  out<<",\"replay\":"<<quote(pendingReplay);
  out<<",\"style\":"<<activeProfile<<",\"iterations\":"<<lastSearchV9.iterations<<",\"nodes\":"<<lastSearchV9.nodes<<",\"depth\":"<<lastSearchV9.depth
  <<",\"passTrials\":"<<lastSearchV9.passTrials<<",\"passOverridden\":"<<(lastSearchV9.passOverridden?"true":"false")<<",\"passMargin\":"<<lastSearchV9.passMargin<<",\"passMs\":"<<lastSearchV9.passMilliseconds
  <<",\"matchPlanNodes\":"<<lastSearchV9.matchPlanNodes<<",\"matchPlanTrials\":["<<lastSearchV9.matchPlanTrials[0]<<','<<lastSearchV9.matchPlanTrials[1]<<','<<lastSearchV9.matchPlanTrials[2]<<']'
  <<",\"characterTieCandidates\":"<<lastSearchV9.characterTieCandidates<<",\"characterTieChanged\":"<<(lastSearchV9.characterTieChanged?"true":"false")<<",\"characterTieMs\":"<<lastSearchV9.characterTieMilliseconds
  <<",\"ms\":"<<lastSearchV9.milliseconds<<",\"reviewTrials\":"<<lastSearchV9.reviewTrials<<",\"reviewMs\":"<<lastSearchV9.reviewMilliseconds<<",\"candidates\":[";
  bool comma=false;for(auto e:lastSearchV9.root){if(comma)out<<',';comma=true;
   out<<"{\"action\":"<<quote(actionKey(e.action))<<",\"visits\":"<<e.visits<<",\"valueSum\":"<<e.value<<'}';}
  out<<"],\"opponentWeights\":[";auto w=opponentWeights(before.p[0].faction);for(int k=0;k<3;k++){if(k)out<<',';out<<w[k];}out<<']';
  // Research model records public evidence but is not a qualified playing
  // policy. Do not claim this posterior was used by the production search.
  auto& plans=memory.roundPlans[before.p[0].faction];auto posterior=plans.weights();
  out<<",\"verifiedLessonMatches\":"<<lastSearchV9.verifiedLessonMatches<<",\"verifiedLessonProof\":"<<(lastSearchV9.verifiedLessonProof?"true":"false");
  out<<",\"contextResponseEnabled\":"<<(lastSearchV9.responseForecastEnabled?"true":"false")<<",\"contextResponseActiveHabits\":"<<lastSearchV9.responseActiveHabits;
  out<<",\"roundPlanModel\":{\"forecastEnabled\":false,\"observations\":"<<plans.observations<<",\"posterior\":[";
  for(int k=0;k<3;k++){if(k)out<<',';out<<posterior[k];}out<<"]}";
 }
 out<<"}\n";
}
void begin(uint64_t seed,int f,int opponent,int leader,int enemyLeader,bool expert,int first,const vector<int>& custom){
 if(f<0||f>4||opponent<0||opponent>4)throw runtime_error("Недоступная фракция");
 Rng buildRng(seed^0x9135);int nextProfile=memory.profile(opponent,f,buildRng);
 if(enemyLeader==-1)enemyLeader=chooseLeader(opponent,f,leader,seed,nextProfile);
 for(auto [who,l]:{pair<int,int>{f,leader},{opponent,enemyLeader}})if(find(factionLeaders[who].begin(),factionLeaders[who].end(),l)==factionLeaders[who].end())throw runtime_error("Лидер не принадлежит фракции");
 auto own=preset(f,leader),enemy=preparedDeck(opponent,f,nextProfile,enemyLeader);if(!custom.empty())own.cards=custom;validateDeck(own);validateDeck(enemy);
 if(realExperience.active())realExperience.finish(game,"abandoned");
 if(!verifiedLessons.path.empty()){auto path=verifiedLessons.path;verifiedLessons={};verifiedLessons.load(path);}
 activeVerifiedLessons=verifiedLessons;for(auto item:starterLessons.cases){auto found=activeVerifiedLessons.cases.find(item.first);if(found!=activeVerifiedLessons.cases.end()){if(found->second.sum*item.second.sum<0)found->second.contradictions++;}else if(activeVerifiedLessons.cases.size()<512)activeVerifiedLessons.cases.insert(item);}
 clearVerifiedProofs();activeProfile=nextProfile;learnedResult=false;for(auto& window:memory.responseWindows)window={};
 traceMatch=to_string(seed);strong=expert;swaps=0;returned.clear();journal.clear();ready=false;begun=true;game=Game();game.rng=Rng(seed);decisions=Rng(seed^97191);decks={own,enemy};library={own,enemy};
 pendingReplay.clear();pendingReplayStep=-1;
 for(int s=0;s<2;s++){game.p[s].faction=decks[s].faction;game.p[s].leader=decks[s].leader;game.p[s].deck=decks[s].cards;game.rng.shuffle(game.p[s].deck);}
 if(leader==4||enemyLeader==4)game.p[0].used=game.p[1].used=true;
 for(int s=0;s<2;s++)draw(game,s,10+(game.p[s].leader==15&&!game.p[s].used?1:0));
 prepareOpening(game,1);game.turn=game.rng.n(2);
 if(f==3&&opponent!=3)game.turn=first;
 else if(opponent==3&&f!=3)game.turn=1;
 else if(f==3&&opponent==3)game.turn=game.rng.n(2)==0?first:1;
 resetSkins();lastSkin=-1;
 logLine("Раздача. Можно заменить до двух карт.");
 // Remember only a valid, fully prepared match; rejected settings do not count.
 memory.rememberProfile(opponent,f,activeProfile);memory.save();
}
void perform(Act a){
 int actor=game.turn;string who=actor==0?"Вы":"Соперник";
 string action=a.kind==3?"пас":a.kind==1?"способность лидера":a.kind==2?"воскрешение: "+C[a.card].name:a.kind==4?"сброс: "+C[a.card].name:a.kind==5?"выбор карты из колоды":C[a.card].name;
 if(a.kind==0&&a.card==DEC)action+=" → "+C[game.p[actor].board[a.target].id].name;
 if(a.kind==1&&a.target>=0)action+=" → "+C[a.target].name;
 Game before=game;auto inv=inventory(game);int oldRound=game.round,oldWinner=game.winner;
 string experienceReplay;
 if(realExperience.active())try{
  if(actor==1&&pendingReplayStep==before.steps)experienceReplay=pendingReplay;
  else{auto seed=realExperience.analysisSeed(before.steps,actor);auto observed=observation(before,actor);
   auto publicLib=publicDeckHypotheses(observed,decks[actor],decks[1-actor].cards.size(),seed);
   experienceReplay=encodeReplay(observed,actor,decks[actor],publicLib,seed,actor?activeProfile:0,{1,1,1},{1,0,0},true,false);}
 }catch(const exception&){realExperience.failed=true;realExperience.out.close();}
 lastSkin=transferSkin(a);apply(game,a);reconcileSkins();if(inventory(game)!=inv)throw runtime_error("Нарушен учёт карт");learnPublicMove(before,game,a);recordDecisionV9(before,a);logLine(who+": "+action);
 realExperience.decision(before,game,a,experienceReplay,activeProfile,lastSearchV9);
 if(game.round!=oldRound)logLine("Раунд "+to_string(game.round+1)+". Осталось рубинов: "+to_string(2-game.p[1].wins)+" : "+to_string(2-game.p[0].wins));
 if(game.winner>=0&&oldWinner<0)decisionLearning.save();
 if(game.winner>=0&&oldWinner<0)logLine(game.winner==2?"Партия закончилась вничью.":game.winner==0?"Партия за вами.":"Партия за соперником.");
}
void arrayJson(ostream& o,const vector<int>& v){o<<'[';for(int i=0;i<(int)v.size();i++){if(i)o<<',';o<<v[i];}o<<']';}
void output(){
 cout<<"{\"ok\":true,\"engineBuild\":"<<quote(ENGINE_BUILD)<<",\"human\":0,\"ready\":"<<(ready?"true":"false")<<",\"swaps\":"<<swaps<<",\"strong\":"<<(strong?"true":"false")<<",\"turn\":"<<game.turn<<",\"pending\":"<<game.pending<<",\"pendingKind\":"<<game.pendingKind<<",\"round\":"<<game.round<<",\"winner\":"<<game.winner<<",\"steps\":"<<game.steps<<",\"players\":[";
 for(int s=0;s<2;s++){if(s)cout<<',';auto& p=game.p[s];cout<<"{\"faction\":"<<p.faction<<",\"score\":"<<score(game,s)<<",\"wins\":"<<p.wins<<",\"lives\":"<<2-game.p[1-s].wins<<",\"leader\":"<<p.leader<<",\"used\":"<<(p.used?"true":"false")<<",\"passed\":"<<(p.passed?"true":"false")<<",\"horn\":"<<p.lhorn<<",\"deckCount\":"<<p.deck.size()<<",\"handCount\":"<<p.hand.size()<<",\"hand\":";arrayJson(cout,s==0?p.hand:vector<int>{});cout<<",\"known\":";arrayJson(cout,p.known);cout<<",\"grave\":";arrayJson(cout,p.grave);cout<<",\"handSkins\":";arrayJson(cout,s==0?skinList(s*5):vector<int>{});cout<<",\"handVariants\":";arrayJson(cout,s==0?variantList(s*5):vector<int>{});cout<<",\"graveVariants\":";arrayJson(cout,variantList(s*5+2));cout<<",\"graveSkins\":";arrayJson(cout,skinList(s*5+2));cout<<",\"board\":[";auto power=powers(game,s);for(int i=0;i<(int)p.board.size();i++){if(i)cout<<',';cout<<"{\"id\":"<<p.board[i].id<<",\"row\":"<<p.board[i].row<<",\"skin\":"<<skins[s*5+3][i].skin<<",\"variant\":"<<skins[s*5+3][i].variant<<",\"power\":"<<power[i]<<",\"index\":"<<i<<"}";}cout<<"]}";}
 cout<<"],\"weather\":"<<weatherMask(game)<<",\"weatherCards\":[";for(int i=0;i<(int)game.weather.size();i++){if(i)cout<<',';cout<<game.weather[i].second;}cout<<"],\"decklist\":";arrayJson(cout,decks[0].cards);
 cout<<",\"lastSkin\":"<<lastSkin<<",\"weatherSkins\":[";{array<int,2> wi={0,0};for(int i=0;i<(int)game.weather.size();i++){if(i)cout<<',';int owner=game.weather[i].first;cout<<skins[owner*5+4][wi[owner]++].skin;}}cout<<"],\"legal\":[";if(ready&&game.turn==0){auto acts=legal(game);for(int i=0;i<(int)acts.size();i++){if(i)cout<<',';auto a=acts[i];cout<<"{\"index\":"<<i<<",\"kind\":"<<a.kind<<",\"card\":"<<a.card<<",\"target\":"<<a.target<<",\"skin\":"<<actionSkin(a)<<",\"variant\":"<<actionVariant(a)<<",\"row\":"<<a.row<<"}";}}
 cout<<"],\"log\":[";for(int i=0;i<(int)journal.size();i++){if(i)cout<<',';cout<<quote(journal[i]);}cout<<"]}"<<endl;
}
#ifndef CLASSIC_NO_MAIN
int main(int argc,char**argv){
 setup();initClassic();starterLessons.load((filesystem::path(argv[0]).parent_path()/"verified-starter-lessons.txt").string());if(argc>3){memory.load(argv[3]);decisionLearning.load(string(argv[3])+".priors-v12");verifiedLessons.load(string(argv[3])+".verified-v1");}originals=loadDecks(argc>1?argv[1]:"decks.txt");
 if(const char* source=getenv("GWENT_EXPERIENCE_SOURCE"))if(string(source)=="protocol_test")realExperience.source="protocol_test";
 if(argc>2&&string(argv[2])=="export"){cout<<"{";for(int f=0;f<5;f++){if(f)cout<<',';auto d=preset(f,factionLeaders[f][0]);validateDeck(d);cout<<quote(to_string(f))<<":";arrayJson(cout,d.cards);}cout<<"}"<<endl;return 0;}
 string line;while(getline(cin,line)){try{istringstream in(line);string cmd;in>>cmd;
 if(cmd=="version"){cout<<"{\"ok\":true,\"engineBuild\":"<<quote(ENGINE_BUILD)<<"}"<<endl;continue;}
 if(cmd=="new"){uint64_t seed;int f,opp,l,el,expert,first;if(!(in>>seed>>f>>opp>>l>>el>>expert>>first)||first<0||first>1)throw runtime_error("Некорректные настройки");string csv;in>>csv;vector<int> custom;if(!csv.empty()&&csv!="-"){stringstream ss(csv);string num;while(getline(ss,num,','))custom.push_back(stoi(num));}begin(seed,f,opp,l,el,expert,first,custom);}
 else if(!begun)throw runtime_error("Сначала начните партию");
 else if(cmd=="swap"){int i;if(!(in>>i)||ready||swaps>=2||i<0||i>=(int)game.p[0].hand.size())throw runtime_error("Замена недоступна");swappedSkins.push_back(takeSkin(0,game.p[0].hand[i],i));returned.push_back(game.p[0].hand[i]);game.p[0].hand.erase(game.p[0].hand.begin()+i);int dealt=game.p[0].drawn;draw(game,0,1);game.p[0].drawn=dealt;reconcileSkins();swaps++;}
 else if(cmd=="ready"){if(ready)throw runtime_error("Партия уже началась");game.p[0].deck.insert(game.p[0].deck.end(),returned.begin(),returned.end());game.rng.shuffle(game.p[0].deck);returned.clear();skins[1].insert(skins[1].end(),swappedSkins.begin(),swappedSkins.end());swappedSkins.clear();reconcileSkins();ready=true;realExperience.open(memory.path,traceMatch,game,activeProfile,strong);logLine(game.turn==0?"Первый ход — ваш.":"Первым ходит соперник.");}
 else if(cmd=="act"){int i;auto acts=legal(game);if(!(in>>i)||!ready||game.turn!=0||i<0||i>=(int)acts.size())throw runtime_error("Недоступный ход");int handIndex=-1;if(in>>handIndex){if(acts[i].kind!=0||handIndex<0||handIndex>=(int)game.p[0].hand.size()||game.p[0].hand[handIndex]!=acts[i].card)throw runtime_error("Недоступная карта руки");auto skin=takeSkin(0,acts[i].card,handIndex);game.p[0].hand.erase(game.p[0].hand.begin()+handIndex);game.p[0].hand.insert(game.p[0].hand.begin(),acts[i].card);skins[0].insert(skins[0].begin(),skin);}perform(acts[i]);}
 else if(cmd=="bot"){if(!ready||game.turn!=1||game.winner>=0)throw runtime_error("Сейчас не ход бота");auto seed=decisions.next();auto response=opponentResponse(game.p[0].faction);auto publicLib=publicDeckHypotheses(observation(game,1),decks[1],decks[0].cards.size(),seed);pendingReplay=encodeReplay(game,1,decks[1],publicLib,seed,activeProfile,opponentWeights(game.p[0].faction),{1,0,0},true,false,true,&response,&activeVerifiedLessons);pendingReplayStep=game.steps;auto a=playV12(game,decks[1],publicLib,seed,strong,activeProfile,opponentWeights(game.p[0].faction),{2048,250,18},true,true,&response,&activeVerifiedLessons).chosen;auto acts=legal(game);if(find(acts.begin(),acts.end(),a)==acts.end())throw runtime_error("Недопустимый ход бота");perform(a);}
 else if(cmd!="view")throw runtime_error("Неизвестная команда");output();
 }catch(const exception& e){cout<<"{\"ok\":false,\"error\":"<<quote(e.what())<<"}"<<endl;}}
}
#endif
