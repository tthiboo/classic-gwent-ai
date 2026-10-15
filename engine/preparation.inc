// Preparation never receives the opponent's hand, draw order or custom deck.
int packageKey(int c){
 if(c==id("Arachas Behemoth"))return 201;
 if(c==id("Gaunter O'Dimm")||c==DARK)return 900;
 if(C[c].flags& BERSERK||c==id("Ermion"))return 901;
 return C[c].flags&(BOND|MUSTER)?(C[c].group>=0?C[c].group:1000+c):2000+c;
}
double rosterValue(int c,int opponent,int style,int leader){
 auto x=C[c];double v=x.p;
 if(x.flags&SPY)v=25-.7*x.p;
 if(x.flags&HERO)v+=3+(style==2?2:0);
 if(x.flags&MED)v+=12;
 if(x.flags&BOND)v+=x.p*.7;
 if(x.flags&MUSTER)v+=4;
 if(x.flags&MORALE)v+=3;
 if(x.flags&UHORN)v+=6;
 if(x.flags&DRAGON)v+=opponent==2?10:5;
 if(x.flags&(RSCORCH|SSCORCH|GSCORCH))v+=4;
 if(x.flags&(COW|KAMBI))v+=5;
 if(x.flags& BERSERK)v+=7;
 if(leader==0&&x.row==2&&!(x.flags&HERO))v+=3;
 if(leader==10&&x.row==0&&!(x.flags&HERO))v+=2;
 if(leader==16&&x.row==1&&!(x.flags&HERO))v+=2;
 if(style==1&&x.flags&(BOND|MUSTER))v+=2;
 if(style==0&&x.flags&(MED|SPY|DRAGON|GSCORCH))v+=1.5;
 return v;
}
Deck buildStrategyDeck(int faction,int opponent,int style,int leader){
 map<int,vector<int>> groups;
 for(int c=0;c<(int)C.size();c++)if(C[c].collectible&&!C[c].kind&&(C[c].faction<0||C[c].faction==faction)){
  // Keep synergy packages complete; ordinary copies remain independent choices.
  bool bundled=(C[c].flags&(BOND|MUSTER|BERSERK))||c==id("Ermion");
  for(int copy=0;copy<C[c].limit;copy++)groups[bundled?packageKey(c):10000+c*10+copy].push_back(c);
 }
 struct Plan{double value=-1e100;vector<int> cards;};array<Plan,23> dp;dp[0].value=0;
 for(auto [key,pack]:groups){int size=pack.size();double value=0;for(int c:pack)value+=rosterValue(c,opponent,style,leader);
  for(int n=22;n>=size;n--)if(dp[n-size].value+value>dp[n].value){dp[n]=dp[n-size];dp[n].value+=value;dp[n].cards.insert(dp[n].cards.end(),pack.begin(),pack.end());}
 }
 if(dp[22].cards.empty())throw runtime_error("no complete deck packages");
 Deck d{"strategy_"+to_string(style),faction,leader,dp[22].cards};
 auto put=[&](int c,int count=1){while(count-->0&&std::count(d.cards.begin(),d.cards.end(),c)<C[c].limit)d.cards.push_back(c);};
 int med=0,spy=0;for(int c:d.cards){med+=!!(C[c].flags&MED);spy+=!!(C[c].flags&SPY);}
 // Special cards must justify diluting the unit/draw engine.
 put(DEC,(opponent<2||med>=3)?2:1);put(CLEAR);put(SCORCH);
 if(style==0)put(id(opponent==0?"Torrential Rain":opponent==3?"Impenetrable Fog":"Biting Frost"));
 if(style==1&&leader!=0&&leader!=10&&leader!=16)put(HORN);
 if(style==2&&opponent<2)put(DEC);
 if(any_of(d.cards.begin(),d.cards.end(),[](int c){return C[c].flags& BERSERK;}))put(MARD);
 validateDeck(d);return d;
}
// Whole-hand utility counts summoned cards once, includes tight-bond/transform
// partners, and discounts redundant controls. It is a heuristic, not a win oracle.
double openingHandValue(const vector<int>& hand,const vector<int>& pool,int faction,int enemyFaction,int leader){
 map<int,int> count;for(int c:hand)count[c]++;
 double value=0;int med=0,dec=0,weather=0,clear=0,sc=0;set<int> summoned;
 auto addSummoned=[&](int source){int key=packageKey(source);if(!summoned.insert(key).second)return;
  for(int c:pool)if((C[c].flags&MUSTER||C[c].flags&BOND)&&packageKey(c)==key){
   if(source!=id("Arachas Behemoth")&&c==id("Arachas Behemoth"))continue;
   if(source==DARK&&c==id("Gaunter O'Dimm"))continue;
   value+=C[c].p;
  }
 };
 for(int c:hand){auto x=C[c];
  if(c==DEC){dec++;continue;}if(c==CLEAR){clear++;continue;}if(c==SCORCH){sc++;continue;}if(x.weather){weather++;continue;}
  if(x.kind){value+=c==HORN?7:count[id("Ermion")]?1:5;continue;}
  double v=x.p+(x.flags&HERO?3:0);
  if(x.flags&SPY)v=25-.7*x.p;
  if(x.flags&MED){med++;v+=8.0/med;}
  if(x.flags&BOND)v*=count[c];
  if(x.flags&MUSTER)addSummoned(c);
  if(x.flags&DRAGON)v+=enemyFaction==2?8:4;
  if(x.flags&MORALE)v+=2;
  if(x.flags&UHORN)v+=5;
  if(x.flags& BERSERK){bool activator=count[MARD]||count[id("Ermion")];v+=activator?C[x.transform].p-x.p:1;}
  if(leader==0&&x.row==2&&!(x.flags&HERO))v+=x.p*.35;
  value+=v;
 }
 value+=dec?((enemyFaction<2?12:med?7:3)+max(0,dec-1)*(enemyFaction<2?4:1)):0;
 value+=clear?4-(clear-1)*2:0;value+=weather?4-(weather-1)*2:0;value+=sc?8+(sc>1?2:0):0;
 return value;
}
// Expected value over the known multiset, NEVER the next position in draw order.
// Two-stage planning: after a random replacement, choose the best second swap.
int openingSwap(const Game& g,int side,int remaining,const vector<int>& returned={}){
 const auto& p=g.p[side];auto utility=[&](const vector<int>& h,const vector<int>& d,const vector<int>& back){auto eventual=d;eventual.insert(eventual.end(),back.begin(),back.end());return openingHandValue(h,eventual,p.faction,g.p[1-side].faction,p.leader);};
 auto oneStep=[&](const vector<int>& h,const vector<int>& d,const vector<int>& back){double best=utility(h,d,back);if(d.empty())return best;map<int,int> draws;for(int c:d)draws[c]++;
  set<int> tried;for(int i=0;i<(int)h.size();i++)if(tried.insert(h[i]).second){double expected=0;for(auto [c,n]:draws){auto hand=h,pool=d;hand[i]=c;erase1(pool,c);auto nextBack=back;nextBack.push_back(h[i]);expected+=n*utility(hand,pool,nextBack)/d.size();}best=max(best,expected);}return best;};
 double best=utility(p.hand,p.deck,returned);int chosen=-1;if(p.deck.empty())return chosen;
 map<int,int> draws;for(int c:p.deck)draws[c]++;set<int> tried;
 for(int i=0;i<(int)p.hand.size();i++)if(tried.insert(p.hand[i]).second){double expected=0;
  for(auto [c,n]:draws){auto h=p.hand,d=p.deck;h[i]=c;erase1(d,c);auto back=returned;back.push_back(p.hand[i]);expected+=n*(remaining>1?oneStep(h,d,back):utility(h,d,back))/p.deck.size();}
  if(expected>best+.01){best=expected;chosen=i;}
 }return chosen;
}
void strategicMulligan(Game& g,int side){vector<int> returned;int dealt=g.p[side].drawn;
 for(int n=2;n>0;n--){int i=openingSwap(g,side,n,returned);if(i<0)break;returned.push_back(g.p[side].hand[i]);g.p[side].hand.erase(g.p[side].hand.begin()+i);draw(g,side,1);}
 g.p[side].deck.insert(g.p[side].deck.end(),returned.begin(),returned.end());g.rng.shuffle(g.p[side].deck);
 g.p[side].drawn=dealt; // Replacements exchange cards; they are not extra resources.
}

void prepareOpening(Game& g,int side){
 // Whole-hand replacement has not qualified for Northern Realms yet.
 if(g.p[side].faction==0)mulligan(g,side);else strategicMulligan(g,side);
}
