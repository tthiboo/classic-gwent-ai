bool historyPreparationEnabled=true;
struct PreparationSignals {array<double,3> rows{};double confidence=0,weather=0,scorch=0,spies=0,medics=0,heroes=0,packages=0;};
PreparationSignals publicPreparationSignals(int opponent){
 auto h=memory.preparation[opponent];
 // Reuse historical public observations when loading a pre-extension file.
 if(h.observations<12){PreparationHistory old;for(int c=0;c<int(C.size());c++)old.observe(c,min(8,memory.seen[opponent][c]));if(old.observations>h.observations)h=old;}
 PreparationSignals s;if(h.observations<12||h.features[3]<=0)return s;
 s.confidence=min(1.0,(h.observations-6)/24.0);double total=h.features[0]+h.features[1]+h.features[2];
 if(total>0)for(int row=0;row<3;row++)s.rows[row]=h.features[row]/total;
 double n=h.features[3];s.heroes=h.features[4]/n;s.spies=h.features[5]/n;s.medics=h.features[6]/n;s.weather=h.features[7]/n;s.scorch=h.features[8]/n;s.packages=h.features[9]/n;return s;
}
Deck historyPreparedDeck(Deck d,int opponent){
 if(!historyPreparationEnabled)return d;auto s=publicPreparationSignals(opponent);
 auto add=[&](int c,int desired){int special=count_if(d.cards.begin(),d.cards.end(),[](int x){return C[x].kind;});while(count(d.cards.begin(),d.cards.end(),c)<min(desired,C[c].limit)&&special<10){d.cards.push_back(c);special++;}};
 int preferred=max_element(s.rows.begin(),s.rows.end())-s.rows.begin();if(!s.confidence)preferred=opponent==0?2:opponent==3||opponent==4?1:0;
 int leaderWeather=d.leader==5?id("Torrential Rain"):d.leader==6?id("Impenetrable Fog"):d.leader==18?id("Biting Frost"):d.leader==11?id(preferred==0?"Biting Frost":preferred==1?"Impenetrable Fog":"Torrential Rain"):-1;
 // A weather-from-deck leader requires a matching card in the deck.
 if(leaderWeather>=0&&!count(d.cards.begin(),d.cards.end(),leaderWeather)){
  if(count_if(d.cards.begin(),d.cards.end(),[](int c){return C[c].kind;})>=10){auto it=find_if(d.cards.begin(),d.cards.end(),[&](int c){return C[c].weather||c==HORN;});if(it!=d.cards.end())d.cards.erase(it);}
  add(leaderWeather,1);
 }
 if(s.confidence){
  if(s.weather>=.10)add(CLEAR,2);
  // Heavy hero usage makes extra weather/scorch dilute the draw engine.
  map<int,int> kept;d.cards.erase(remove_if(d.cards.begin(),d.cards.end(),[&](int c){return s.heroes>.45&&c!=leaderWeather&&(C[c].weather||c==SCORCH)&&++kept[c]>1;}),d.cards.end());
  if(s.packages>.18&&s.rows[preferred]>.50&&s.heroes<.35){
   int weather=id(preferred==0?"Biting Frost":preferred==1?"Impenetrable Fog":"Torrential Rain");int ownRows[3]{};for(int c:d.cards)if(!C[c].kind&&!(C[c].flags&(HERO|SPY)))ownRows[C[c].row==3?1:C[c].row]+=C[c].p;
   // Do not add a counter whose main collateral is our own planned army.
   if(ownRows[preferred]*2<accumulate(begin(ownRows),end(ownRows),0))add(weather,1);
  }
  auto b=memory.responses[opponent].habits[3];bool earlyCounter=b.active()&&b.weights()[1]>b.weights()[2];
  if(earlyCounter||s.weather+s.scorch>.22){
   int weakest=-1,hero=-1;for(int i=0;i<int(d.cards.size());i++)if(!C[d.cards[i]].kind&&!C[d.cards[i]].flags&&(weakest<0||C[d.cards[i]].p<C[d.cards[weakest]].p))weakest=i;
   for(int c=0;c<int(C.size());c++)if(C[c].collectible&&(C[c].flags&HERO)&&(C[c].faction<0||C[c].faction==d.faction)&&!count(d.cards.begin(),d.cards.end(),c)&&(hero<0||C[c].p>C[hero].p))hero=c;
   if(weakest>=0&&hero>=0&&C[hero].p>C[d.cards[weakest]].p)d.cards[weakest]=hero;
  }
 }
 validateDeck(d);return d;
}
double historyLeaderBonus(int leader,int opponent,const Deck& own){
 if(!historyPreparationEnabled)return 0;auto s=publicPreparationSignals(opponent);if(!s.confidence)return 0;
 double weather=min(1.0,s.weather/.18),counters=min(1.0,(s.weather+s.scorch)/.25),bonus=0;
 if(leader==1)bonus+=18*weather;
 if(leader==21)bonus+=16*weather;
 if(leader==4)bonus+=13*counters; // public counter habits, no current leader leak
 if(leader==3)bonus+=min(7.0,30*(s.spies+s.medics))-4*s.heroes;
 if(leader==20)bonus+=min(8.0,30*(s.spies+s.medics))-5*weather;
 int row=leader==2?1:leader==7?2:leader==17?0:-1;
 if(row>=0)bonus+=20*s.rows[row]*(1-s.heroes)*min(1.0,s.packages/.2);
 if(leader==5||leader==6||leader==11||leader==18){int target=leader==5?2:leader==6?1:leader==18?0:max_element(s.rows.begin(),s.rows.end())-s.rows.begin();bonus+=12*s.rows[target]*(1-s.heroes);}
 // Avoid rating an unusable ability as if it were available.
 if((leader==5||leader==6||leader==11||leader==18)&&!any_of(own.cards.begin(),own.cards.end(),[](int c){return C[c].weather;}))bonus-=12;
 return s.confidence*bonus;
}
