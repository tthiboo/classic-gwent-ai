// Transparent, offline-trained relative position value. Features deliberately
// use only our hand/multiset and public state, never enemy concealed identities.
const vector<string> VALUE_FEATURES={
 "score_own","score_enemy","score_margin","hand_own","hand_enemy","hand_margin",
 "deck_own","deck_enemy","wins_own","wins_enemy","round","leader_own","leader_enemy",
 "passed_own","passed_enemy","initiative","pending_own","pending_enemy",
 "hand_power","hand_heroes","hand_ordinary","spies","decoys","medics","weather_hand",
 "control","horns","morale","bond_partners","muster_reserve","grave_own","grave_enemy",
 "row_own_0","row_own_1","row_own_2","row_enemy_0","row_enemy_1","row_enemy_2",
 "weather_0","weather_1","weather_2","own_nr","own_ng","own_monsters","own_scoiatael","own_skellige",
 "enemy_nr","enemy_ng","enemy_monsters","enemy_scoiatael","enemy_skellige",
 "margin_matchpoint","cards_matchpoint","power_matchpoint","margin_enemy_passed","cards_enemy_passed",
 "spies_with_deck","medics_with_grave","horn_row_potential","terminal"
};
vector<double> valueFeatures(const Game& g,int side){
 if(side<0||side>1)throw runtime_error("invalid value perspective");
 int enemy=1-side;const auto& p=g.p[side];const auto& e=g.p[enemy];
 double margin=(score(g,side)-score(g,enemy))/60.0,cards=(int(p.hand.size())-int(e.hand.size()))/12.0;
 vector<double> x={score(g,side)/60.0,score(g,enemy)/60.0,margin,p.hand.size()/12.0,e.hand.size()/12.0,cards,
 p.deck.size()/30.0,e.deck.size()/30.0,p.wins/2.0,e.wins/2.0,g.round/2.0,
 double(!p.used),double(!e.used),double(p.passed),double(e.passed),double(g.turn==side),double(g.pending==side),double(g.pending==enemy)};
 double power=0,heroes=0,ordinary=0,spies=0,decoys=0,medics=0,weather=0,control=0,horns=0,morale=0,muster=0;
 map<int,int> bonds;
 for(int c:p.hand){if(c<0||c>=(int)C.size())throw runtime_error("unknown own value card");auto card=C[c];
  if(!card.kind&&!(card.flags&SPY)){power+=card.p;if(card.flags&HERO)heroes+=card.p;else ordinary+=card.p;}
  spies+=!!(card.flags&SPY);decoys+=c==DEC;medics+=!!(card.flags&MED);weather+=!!card.weather||c==CLEAR;
  control+=c==SCORCH||!!(card.flags&(DRAGON|RSCORCH|SSCORCH|GSCORCH));horns+=c==HORN||!!(card.flags&UHORN);morale+=!!(card.flags&MORALE);
  if(card.flags&BOND)bonds[card.group>=0?card.group:c]++;
 }
 for(auto u:p.board)if(C[u.id].flags&BOND)bonds[C[u.id].group>=0?C[u.id].group:u.id]++;
 double partners=0;for(auto pair:bonds)partners+=max(0,pair.second-1);
 for(int c:p.deck)if(c>=0&&C[c].flags&MUSTER)muster++;
 double ownGrave=graveRecoveryValue(g,side)/60.0,enemyGrave=graveRecoveryValue(g,enemy)/60.0;
 for(double v:{power/80,heroes/40,ordinary/60,spies/3,decoys/3,medics/3,weather/3,control/3,horns/3,morale/3,partners/3,muster/8,ownGrave,enemyGrave})x.push_back(v);
 array<double,3> ownRows{},enemyRows{};
 for(int s:{side,enemy}){auto v=powers(g,s);auto& rows=s==side?ownRows:enemyRows;for(int i=0;i<(int)v.size();i++)rows[g.p[s].board[i].row]+=v[i];for(double r:rows)x.push_back(r/40);}
 for(int row=0;row<3;row++)x.push_back(!!(weatherMask(g)&(1<<row)));
 for(int f=0;f<5;f++)x.push_back(p.faction==f);for(int f=0;f<5;f++)x.push_back(e.faction==f);
 for(double v:{margin*bool(e.wins),cards*bool(e.wins),power/80*bool(e.wins),margin*e.passed,cards*e.passed,
 spies/3*min(2,int(p.deck.size()))/2,medics/3*ownGrave,horns/3*(*max_element(ownRows.begin(),ownRows.end()))/40,
 g.winner<0?0.0:g.winner==2?0.0:g.winner==side?1.0:-1.0})x.push_back(v);
 if(x.size()!=VALUE_FEATURES.size())throw runtime_error("value feature schema mismatch");
 for(double& v:x)v=clamp(v,-4.0,4.0);return x;
}
uint64_t valueSchema(){uint64_t hash=1469598103934665603ULL;for(auto name:VALUE_FEATURES)for(unsigned char c:name+";"){hash^=c;hash*=1099511628211ULL;}return hash;}
struct PositionValue {
 vector<double> weights;bool enabled=false;
 bool load(const string& path){
  enabled=false;weights.clear();ifstream in(path,ios::binary|ios::ate);if(!in||in.tellg()>65536)return false;in.seekg(0);
  string schema;uint64_t fingerprint=0;int size=0;if(!(in>>schema>>fingerprint>>size)||schema!="GWENT_VALUE_1"||fingerprint!=valueSchema()||size!=(int)VALUE_FEATURES.size())return false;
  vector<double> candidate(size);for(double& w:candidate)if(!(in>>w)||!isfinite(w)||abs(w)>10)return false;
  string extra;if(in>>extra)return false;weights=std::move(candidate);enabled=true;return true;
 }
 double relative(const vector<double>& x)const{if(!enabled)return 0;if(x.size()!=weights.size())throw runtime_error("invalid value features");double v=0;for(int i=0;i<(int)x.size();i++)v+=x[i]*weights[i];return clamp(v,-1.0,1.0);}
} positionValue;
