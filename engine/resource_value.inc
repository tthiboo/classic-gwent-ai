#pragma once
// Research representation of the position AFTER a move. Own remaining cards
// and public board/graves only: never enemy concealed identities or draw order.
// Interactions express spendable combinations, next-round recovery and context.
vector<double> resourceFeatures(const Game& g,int side,int style){
 if(style<0||style>2)throw runtime_error("invalid resource style");
 auto x=valueFeatures(g,side);const auto& p=g.p[side];const auto& e=g.p[1-side];
 map<int,pair<int,double>> bonds,musters;set<int> musterAccess;
 double medics=0,spies=0,decoys=0,horns=0,boardRecovery=0,spyExposure=0;
 auto group=[](int c){return C[c].group>=0?C[c].group:c;};
 for(int c:p.hand){const auto& a=C[c];
  if(a.flags&BOND){auto& b=bonds[group(c)];b.first++;b.second+=a.p;}
  if(a.flags&MUSTER){auto& b=musters[group(c)];b.first++;b.second+=a.p;musterAccess.insert(group(c));}
  medics+=bool(a.flags&MED);spies+=bool(a.flags&SPY);decoys+=c==DEC;horns+=c==HORN||bool(a.flags&UHORN);
 }
 for(int c:p.deck)if(c>=0&&(C[c].flags&MUSTER)){auto& b=musters[group(c)];b.first++;b.second+=C[c].p;}
 for(auto u:p.board){auto a=C[u.id];if(a.flags&BOND){auto& b=bonds[group(u.id)];b.first++;b.second+=a.p;}
  if(revivable(u.id))boardRecovery=max(boardRecovery,quality(u.id));if(a.flags&SPY)spyExposure+=min(2,int(e.deck.size()));}
 double bondPower=0,musterPower=0;for(auto b:bonds)if(b.second.first>1)bondPower+=b.second.second*b.second.first;
 for(int group:musterAccess)musterPower+=musters[group].second;
 array<double,3> ordinary{};double ownMax=0,enemyMax=0;
 for(int s:{side,1-side}){auto strength=powers(g,s);for(int i=0;i<(int)strength.size();i++)if(!(C[g.p[s].board[i].id].flags&HERO)){
  if(s==side)ordinary[g.p[s].board[i].row]+=strength[i];auto& maximum=s==side?ownMax:enemyMax;maximum=max(maximum,double(strength[i]));}}
 double weatherExposure=0;for(int row=0;row<3;row++)if(weatherMask(g)&(1<<row))weatherExposure+=ordinary[row];
 double grave=graveRecoveryValue(g,side),future= g.round<2?max(grave,boardRecovery):grave;
 for(double v:{bondPower/80,musterPower/60,medics*future/80,spies*min(2,int(p.deck.size()))/6,
  decoys*spyExposure/6,horns*(*max_element(ordinary.begin(),ordinary.end()))/80,
  weatherExposure/60,(ownMax-enemyMax)/30,p.faction==4&&g.round<2?future/60:0,
  !p.used&&p.leader==3?graveRecoveryValue(g,1-side)/60:0})x.push_back(clamp(v,-4.0,4.0));
 // These are contextual preferences, not a card-id lookup table. No arbitrary
 // card copies receive separate weights; unseen combinations can share evidence.
 const array<int,16> contextual={2,5,18,19,21,22,23,25,28,29,30,57,60,61,62,65};
 for(int s=0;s<3;s++)for(int i:contextual)x.push_back(style==s?x[i]:0);
 const array<int,8> matchup={2,5,23,25,28,30,61,62};
 for(int f=0;f<5;f++)for(int i:matchup)x.push_back(e.faction==f?x[i]:0);
 if(x.size()!=158)throw runtime_error("resource schema mismatch");return x;
}
uint64_t resourceSchema(){uint64_t hash=valueSchema();string name="GWENT_RESOURCE_1:bond,muster,medic,spy,decoy,horn,weather,scorch,skellige,emhyr:style16:enemy8";
 for(unsigned char c:name){hash^=c;hash*=1099511628211ULL;}return hash;}
