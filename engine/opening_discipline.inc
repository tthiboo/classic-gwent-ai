// Opening resources are public counts plus our own hand, never the enemy hand.
// A rollout's eventual win is not evidence that donating an uncontested round
// or spending a reactive unit before drawing information was a good decision.
void openingDiscipline(const Game& g,vector<Act>& actions){
 int s=g.turn,o=1-s;const auto& p=g.p[s];
 if(g.round||g.pending>=0||g.opening||g.winner>=0||p.wins||g.p[o].wins||g.p[o].passed)return;
 int spent=max(0,p.drawn-int(p.hand.size()));
 if(spent>2||p.hand.empty())return;
 vector<Act> harvest,removal;bool productive=false;int maxGain=0;
 const int margin=score(g,s)-score(g,o);
 for(auto a:actions)if(a.kind!=3){
  if(a.kind==0&&(C[a.card].flags&SPY)&&!p.deck.empty()){harvest.push_back(a);productive=true;continue;}
  // Probe visible points only. Canonicalise our deck order and remove the
  // concealed enemy deck; never value the identities of hypothetical draws.
  Game z=observation(g,s);sort(z.p[s].deck.begin(),z.p[s].deck.end());z.p[o].deck.clear();
  apply(z,a);int gain=score(z,s)-score(z,o)-margin;maxGain=max(maxGain,gain);
  productive|=gain>0;
  if(score(g,o)-score(z,o)>=10)removal.push_back(a);
 }
 bool fresh=spent==0,cheapContest=p.hand.size()>=7&&g.p[o].hand.size()>=5&&margin<=12&&margin>=-maxGain;
 if(productive&&(fresh||cheapContest))actions.erase(remove_if(actions.begin(),actions.end(),[](Act a){return a.kind==3;}),actions.end());
 // In a live opening, obtain the two unknown cards before committing a hero
 // or an untriggered Villentretenmerth. A useful immediate removal survives.
 if(!harvest.empty()&&p.hand.size()>=7){
  actions.erase(remove_if(actions.begin(),actions.end(),[&](Act a){return find(harvest.begin(),harvest.end(),a)==harvest.end()&&find(removal.begin(),removal.end(),a)==removal.end();}),actions.end());
 }
}
