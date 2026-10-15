// Research-only frozen continuation transfer. Preserve own resource counts and
// every ability-bearing identity. Ordinary collectible units can share a class
// only at identical printed power/row; counts in EVERY visible zone stay exact.
// This is a policy approximation, not an exact endgame proof.
struct ContinuationTransferRecord {string context;vector<string> bindings;};
string continuationUnit(int card){
 const auto& c=C[card];bool plain=c.collectible&&!c.kind&&!c.flags&&c.group<0&&c.transform<0;
 return plain?"plain:"+to_string(c.p)+":"+to_string(c.row):"id:"+to_string(card);
}
void continuationCards(ostringstream& out,const vector<int>& cards){
 vector<string> tokens;for(int c:cards)tokens.push_back(continuationUnit(c));
 sort(tokens.begin(),tokens.end());for(auto& token:tokens)out<<token<<',';out<<'/';
}
string continuationBinding(const Game& g,Act a){
 if(a.kind==0&&a.card==DEC){
  if(a.target<0||a.target>=int(g.p[g.turn].board.size()))return "INVALID";
  auto u=g.p[g.turn].board[a.target];
  return "decoy:"+continuationUnit(u.id)+":"+to_string(u.row);
 }
 if(a.kind==0&&a.card>=0&&continuationUnit(a.card).find("plain:")==0)return "play:"+continuationUnit(a.card)+":"+to_string(a.row);
 return actionKey(a);
}
string continuationTransferContext(const Game& g,int plan){
 int own=g.turn;ostringstream out;
 out<<"TRANSFER_2:"<<own<<','<<g.pending<<','<<g.pendingKind<<','<<g.remaining<<','<<g.round<<','<<g.winner<<','
 <<g.emhyrSpecial<<','<<g.heroThreshold<<','<<g.opening<<','<<g.openingTurn<<','<<plan<<';';
 for(int side=0;side<2;side++){
  const auto& p=g.p[side];out<<p.faction<<','<<p.leader<<','<<p.wins<<','<<p.lhorn<<','<<p.used<<','<<p.passed<<','
  <<p.hand.size()<<','<<p.deck.size()<<','<<p.drawn<<','<<p.spies<<','<<p.revives<<':';
  continuationCards(out,p.grave);continuationCards(out,p.known);
  if(side==own){continuationCards(out,p.hand);continuationCards(out,p.deck);}
  vector<string> board;
  for(auto u:p.board)board.push_back(continuationUnit(u.id)+":"+to_string(u.row));
  sort(board.begin(),board.end());for(auto& u:board)out<<u<<';';out<<'|';
 }
 auto weather=g.weather;sort(weather.begin(),weather.end());for(auto w:weather)out<<w.first<<':'<<w.second<<';';
 for(auto q:g.openingQueue)out<<q.first<<':'<<q.second<<';';
 for(auto [c,n]:g.changes)out<<c<<':'<<n<<';';
 return out.str();
}
struct FrozenContinuationTransfer {
 struct Evidence {string binding;int sources=0,visits=0,chosenVisits=0;bool conflict=false;};
 unordered_map<string,Evidence> contexts;
 FrozenContinuationTransfer(const unordered_map<string,NodeV9>& nodes,const unordered_map<string,ContinuationTransferRecord>& records){
  // Construction reads discovery statistics once. Validation cannot change
  // votes or outcomes; conflicting supported recommendations abstain.
  vector<string> keys;for(auto& entry:records)keys.push_back(entry.first);sort(keys.begin(),keys.end());
  for(auto& key:keys){auto found=nodes.find(key);if(found==nodes.end())continue;const auto& n=found->second;
   const auto& record=records.at(key);if(n.visits<4||n.edges.empty()||record.bindings.size()!=n.edges.size())continue;
   int best=0;for(int i=1;i<int(n.edges.size());i++)if(n.edges[i].visits>n.edges[best].visits||
    (n.edges[i].visits==n.edges[best].visits&&n.edges[i].prior>n.edges[best].prior))best=i;
   if(n.edges[best].visits<2)continue;
   auto& e=contexts[record.context];if(e.sources&&e.binding!=record.bindings[best])e.conflict=true;
   if(!e.sources)e.binding=record.bindings[best];e.sources++;e.visits+=n.visits;e.chosenVisits+=n.edges[best].visits;
  }
 }
 template<class Tree> Act replay(const Tree& tree,const Game& g,Rng& rng,int style,int plan,int& source)const{
  source=0;
  // Retain dedicated choice chains and proved catch-up decisions, even when
  // an approximate context happens to have a supported recommendation.
  if(g.pending>=0||g.opening)return fastV8(g,rng,style,64);
  auto proof=catchUp(g,64);if(proof.complete)return proof.first;
  auto key=informationKeyV9(g,g.turn,tree.roundResources);if(tree.matchPlans)key+="/plan:"+to_string(plan);
  auto exact=tree.nodes.find(key);
  if(exact!=tree.nodes.end()&&exact->second.visits>=8&&!exact->second.edges.empty()){
   auto best=max_element(exact->second.edges.begin(),exact->second.edges.end(),[](const auto& a,const auto& b){return a.visits!=b.visits?a.visits<b.visits:a.prior<b.prior;});
   if(best->visits>=4){source=1;return tree.replayPolicy(g,rng,style,plan);}
  }
  auto found=contexts.find(continuationTransferContext(g,plan));
  if(found!=contexts.end()){
   const auto& e=found->second;
   if(!e.conflict&&e.sources>=2&&e.visits>=16&&e.chosenVisits>=8){
    // Rebind to the CURRENT safe actions. Never transplant a board index,
    // resurrection row or leader target. Pass/leader decisions do not transfer.
    for(auto a:safeActionsV9(g))if(a.kind==0&&continuationBinding(g,a)==e.binding){source=2;return a;}
   }
  }
  return tree.replayPolicy(g,rng,style,plan);
 }
};
