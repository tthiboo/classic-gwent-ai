#ifndef GWENT_COHERENT_ROSTER
#define GWENT_COHERENT_ROSTER
// Complete packages are plausible opponents, not leaked custom deck lists.
// Publicly required copies are retained, with a broad independent-card mixture
// still supplied by the caller. All leaders here are already public in play.
optional<Deck> coherentPublicRoster(int faction,int opponent,int leader,int total,
 const vector<int>& mandatory,int style,uint64_t seed){
 if(total<22||total>70)return {};
 Deck d{"structured_public",faction,leader,mandatory};Rng rng(seed);
 int units=0,specials=0;map<int,int> counts;
 for(int c:mandatory){auto x=C[c];if(!x.collectible||(x.faction>=0&&x.faction!=faction)||++counts[c]>x.limit)return {};units+=!x.kind;specials+=!!x.kind;}
 if(specials>10)return {};
 int target=clamp(total-(style==0?5:style==1?4:6),max({22,units,total-10}),total-specials);
 int need=target-units;if(need<0||need>70)return {};
 map<int,vector<int>> packs;
 for(int c=0;c<int(C.size());c++){auto x=C[c];if(!x.collectible||x.kind||(x.faction>=0&&x.faction!=faction))continue;
  bool package=(x.flags&(BOND|MUSTER|BERSERK))||c==id("Ermion");
  for(int copy=counts[c];copy<x.limit;copy++)packs[package?packageKey(c):10000+c*10+copy].push_back(c);
 }
 struct Entry{double value=-1e100;vector<int> cards;};vector<Entry> dp(need+1);dp[0].value=0;
 for(auto item:packs){auto pack=item.second;int n=pack.size();if(n>need)continue;double value=0;
  for(int c:pack)value+=rosterValue(c,opponent,style,leader)+rng.u()*2;
  bool publicPartner=any_of(mandatory.begin(),mandatory.end(),[&](int c){return packageKey(c)==item.first;});if(publicPartner)value+=12;
  for(int k=need;k>=n;k--)if(dp[k-n].value+value>dp[k].value){dp[k]=dp[k-n];dp[k].value+=value;dp[k].cards.insert(dp[k].cards.end(),pack.begin(),pack.end());}
 }
 if(dp[need].value< -1e90)return {};d.cards.insert(d.cards.end(),dp[need].cards.begin(),dp[need].cards.end());
 auto support=buildStrategyDeck(faction,opponent,style,leader);
 while(int(d.cards.size())<total){vector<int> options;vector<double> weights;double sum=0;
  for(int c=0;c<int(C.size());c++){auto x=C[c];if(!x.collectible||!x.kind||(x.faction>=0&&x.faction!=faction)||count(d.cards.begin(),d.cards.end(),c)>=x.limit)continue;
   double weight=count(support.cards.begin(),support.cards.end(),c)?4:1;if(c==MARD&&!any_of(d.cards.begin(),d.cards.end(),[](int c){return C[c].flags& BERSERK;}))weight=.1;
   options.push_back(c);weights.push_back(weight);sum+=weight;
  }
  if(options.empty())return {};double roll=rng.u()*sum;int selected=options.back();for(int k=0;k<int(options.size());k++)if((roll-=weights[k])<=0){selected=options[k];break;}d.cards.push_back(selected);
 }
 try{validateDeck(d);}catch(const exception&){return {};}return d;
}

#endif
