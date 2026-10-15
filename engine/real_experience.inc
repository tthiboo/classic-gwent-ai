#include <filesystem>
// Local observer-specific episodes. This logger cannot select a move, consume
// game/search RNG, or supply the other observer's private cards to a policy.
// One atomic directory per match keeps abandoned/crashed episodes separate.
struct RealExperience {
 ofstream out;string match,folder;int records=0;size_t bytes=0;
 size_t maximumEpisodeBytes=4*1024*1024,maximumTotalBytes=512*1024*1024,maximumEpisodes=1024;
 bool failed=false;string source="human_vs_bot";
 bool active()const{return out.is_open()&&!failed;}
 void write(const string& text){
  if(!active())return;
  if(bytes+text.size()+1>maximumEpisodeBytes){failed=true;out.close();return;}
  out<<text<<'\n';out.flush();bytes+=text.size()+1;
  if(!out){failed=true;out.close();}
 }
 void finish(const Game& g,const string& status){
  if(!out.is_open())return;
  ostringstream s;s<<"{\"schema\":1,\"kind\":\"real_game_end_v1\",\"match\":"<<quote(match)
   <<",\"status\":"<<quote(status)<<",\"winner\":"<<g.winner<<",\"steps\":"<<g.steps<<",\"records\":"<<records<<",\"publicFinal\":"<<quote(publicKey(g))<<'}';
  write(s.str());out.close();failed=failed||out.fail();
  if(!failed){error_code ec;filesystem::rename(folder+"/episode.jsonl.part",folder+"/episode.jsonl",ec);failed=bool(ec);}
 }
 void open(const string& memoryPath,const string& dealGroup,const Game& g,int profile,bool expert){
  if(out.is_open()){failed=true;out.close();return;}
  out.clear();
  match.clear();folder.clear();records=0;bytes=0;failed=false;
  if(memoryPath.empty())return;
  error_code ec;string root=memoryPath+".experience";filesystem::create_directories(root,ec);if(ec)return;
  size_t total=0,count=0;
  for(filesystem::directory_iterator it(root,ec),end;!ec&&it!=end;it.increment(ec)){
   if(!it->is_directory(ec))continue;count++;
   for(const auto& name:{"episode.jsonl","episode.jsonl.part"}){
    error_code sizeError;auto n=filesystem::file_size(it->path()/name,sizeError);if(!sizeError)total+=n;
   }
  }
  if(ec||count>=maximumEpisodes||total>=maximumTotalBytes)return;
  static uint64_t serial=0;
  for(int attempt=0;attempt<8;attempt++){
   match=to_string(chrono::system_clock::now().time_since_epoch().count())+"-"+to_string(++serial);
   folder=root+"/"+match;if(filesystem::create_directory(folder,ec))break;
   if(ec)return;folder.clear();
  }
  if(folder.empty())return;
  out.open(folder+"/episode.jsonl.part",ios::out);if(!out){out.close();return;}
  ostringstream s;s<<"{\"schema\":1,\"kind\":\"real_game_start_v1\",\"match\":"<<quote(match)<<",\"dealGroup\":"<<quote(dealGroup)
   <<",\"source\":"<<quote(source)<<",\"catalog\":"<<quote(to_string(replayCatalog()))<<",\"policy\":\"classic_v12\",\"expert\":"<<(expert?"true":"false")
   <<",\"botStyle\":"<<profile<<",\"controllers\":[\"human\",\"bot\"],\"startStep\":"<<g.steps<<",\"publicInitial\":"<<quote(publicKey(g))<<'}';
  write(s.str());
 }
 uint64_t analysisSeed(int step,int side)const{
  uint64_t h=1469598103934665603ULL;
  for(unsigned char c:match){h^=c;h*=1099511628211ULL;}
  return h^(uint64_t(step+1)*9151ULL)^(uint64_t(side+1)*0x936db614e397ULL);
 }
 void decision(const Game& before,const Game& after,Act a,const string& replay,int profile,const ReportV9& search){
  if(!active())return;
  ostringstream s;s.precision(17);int side=before.turn;
  s<<"{\"schema\":1,\"kind\":\"real_game_decision_v1\",\"match\":"<<quote(match)<<",\"position\":"<<quote(match+":"+to_string(side)+":"+to_string(before.steps))
   <<",\"index\":"<<records<<",\"step\":"<<before.steps<<",\"stepAfter\":"<<after.steps<<",\"actor\":"<<side<<",\"controller\":"<<quote(side?"bot":"human")
   <<",\"style\":"<<(side?to_string(profile):"null")<<",\"analysisStyle\":"<<(side?profile:0)
   <<",\"action\":"<<quote(actionKey(a))<<",\"replay\":"<<quote(replay)<<",\"publicBefore\":"<<quote(publicKey(before))<<",\"publicAfter\":"<<quote(publicKey(after))
   <<",\"winnerAfter\":"<<after.winner<<",\"faction\":"<<before.p[side].faction<<",\"enemyFaction\":"<<before.p[1-side].faction<<",\"round\":"<<before.round;
  // Human choices have no invented search statistics or assigned character.
  if(side){s<<",\"search\":{\"iterations\":"<<search.iterations<<",\"milliseconds\":"<<search.milliseconds<<",\"roots\":[";
   for(int i=0;i<int(search.root.size());i++){if(i)s<<',';auto e=search.root[i];s<<"{\"action\":"<<quote(actionKey(e.action))<<",\"prior\":"<<e.prior<<",\"visits\":"<<e.visits<<",\"valueSum\":"<<e.value<<'}';}s<<"]}";
  }
  s<<'}';write(s.str());records++;
  if(after.winner>=0)finish(after,"completed");
 }
} realExperience;
