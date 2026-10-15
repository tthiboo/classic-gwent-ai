// Presentation-only physical-card ledger. Never passed to search or belief models.
struct SkinCard { int id,skin,row=-1,variant=0; };
array<vector<SkinCard>,10> skins; // per side: hand, deck, grave, board, weather
vector<SkinCard> swappedSkins;
vector<pair<int,SkinCard>> queuedSkins;
vector<int> zoneIds(int z){int side=z/5,zone=z%5;auto&p=game.p[side];if(zone==0)return p.hand;if(zone==1)return p.deck;if(zone==2)return p.grave;vector<int> ids;if(zone==3){for(auto u:p.board)ids.push_back(u.id);}else for(auto w:game.weather)if(w.first==side)ids.push_back(w.second);return ids;}
void resetSkins(){array<map<int,int>,2> copies;for(int z=0;z<10;z++){skins[z].clear();auto ids=zoneIds(z);for(int i=0;i<(int)ids.size();i++)skins[z].push_back({ids[i],game.p[z/5].faction,z%5==3?game.p[z/5].board[i].row:-1,copies[z/5][ids[i]]++});}swappedSkins.clear();queuedSkins.clear();}
SkinCard takeSkin(int zone,int id,int index=-1){auto&v=skins[zone];if(index<0){for(int i=0;i<(int)v.size();i++)if(v[i].id==id){index=i;break;}}if(index<0||index>=(int)v.size())throw runtime_error("Card artwork ledger mismatch");auto c=v[index];v.erase(v.begin()+index);return c;}
void moveSkin(int from,int to,int id){skins[to].push_back(takeSkin(from,id));}
// Explicit player-selected transfers happen before reconciling automatic effects.
int transferSkin(Act a){int side=game.turn,z=side*5,skin=game.p[side].faction;
 if(a.kind==0||a.kind==2){auto c=takeSkin(z+(a.kind==2?2:0),a.card);skin=c.skin;c.row=a.row<0?C[a.card].row:a.row;
  if(a.kind==0&&a.card==DEC){auto u=takeSkin(z+3,game.p[side].board[a.target].id,a.target);skins[z].push_back(u);c.row=u.row;skins[z+3].insert(skins[z+3].begin()+a.target,c);}
  else if(!C[a.card].kind)skins[((C[a.card].flags&SPY)?1-side:side)*5+3].push_back(c);
  else skins[z+(C[a.card].weather?4:(a.card==HORN||a.card==MARD)?3:2)].push_back(c);
 }else if(a.kind==4)moveSkin(z,z+2,a.card);
 else if(a.kind==5)moveSkin(z+1,z,a.card);
 else if(a.kind==1){int l=game.p[side].leader;if(l==3||l==13)moveSkin((l==3?1-side:side)*5+2,z,a.target);else if(l==5||l==6||l==11||l==18){auto c=takeSkin(z+1,a.target);skin=c.skin;skins[z+4].push_back(c);}}
 return skin;
}
void reconcileSkins(){
 array<vector<SkinCard>,10> next;vector<pair<int,SkinCard>> removed=queuedSkins;queuedSkins.clear();
 for(int z=0;z<10;z++){auto ids=zoneIds(z);auto old=skins[z];vector<bool> used(old.size());
  for(int j=0;j<(int)ids.size();j++){int id=ids[j],row=z%5==3?game.p[z/5].board[j].row:-1;int k=-1;for(int i=0;i<(int)old.size();i++)if(!used[i]&&old[i].id==id&&(row<0||old[i].row==row)){k=i;break;}if(k>=0){used[k]=true;next[z].push_back(old[k]);}else next[z].push_back({id,-1,row});}
  for(int i=0;i<(int)old.size();i++)if(!used[i])removed.push_back({z,old[i]});
 }
 for(int z=0;z<10;z++)for(auto&c:next[z])if(c.skin<0){
  int best=-1;for(int i=0;i<(int)removed.size();i++)if(removed[i].second.id==c.id){if(best<0||removed[i].first/5==z/5)best=i;if(removed[i].first/5==z/5)break;}
  if(best>=0){c.skin=removed[best].second.skin;c.variant=removed[best].second.variant;removed.erase(removed.begin()+best);}else c.skin=game.p[z/5].faction; // spawned/transformed faction units
 }
 for(auto queued:game.openingQueue){int side=queued.first,id=queued.second;auto it=find_if(removed.begin(),removed.end(),[&](auto item){return item.second.id==id&&item.first/5==side;});if(it!=removed.end()){queuedSkins.push_back(*it);removed.erase(it);}else queuedSkins.push_back({side*5+2,{id,game.p[side].faction}});}
 skins=std::move(next);
}
vector<int> skinList(int zone){vector<int> v;for(auto c:skins[zone])v.push_back(c.skin);return v;}

int actionSkin(Act a){int side=game.turn,z=side*5,id=a.card;if(a.kind==1){id=a.target;z+=(game.p[side].leader==3?5-10*side:0);z+=2;}else z+=a.kind==2?2:a.kind==5?1:0;for(auto c:skins[z])if(c.id==id)return c.skin;return game.p[side].faction;}

vector<int> variantList(int zone){vector<int> v;for(auto c:skins[zone])v.push_back(c.variant);return v;}

int actionVariant(Act a){int side=game.turn,z=side*5,id=a.card;if(a.kind==1){id=a.target;z+=(game.p[side].leader==3?5-10*side:0);z+=2;}else z+=a.kind==2?2:a.kind==5?1:0;for(auto c:skins[z])if(c.id==id)return c.variant;return 0;}
