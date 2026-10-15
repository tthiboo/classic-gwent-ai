#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
int main(){setup();initClassic();originals=loadDecks("decks.txt");int decksBuilt=0;
 for(int f=0;f<5;f++)for(int o=0;o<5;o++)for(int st=0;st<3;st++)for(int l:factionLeaders[f]){auto d=buildStrategyDeck(f,o,st,l);validateDeck(d);assert(count_if(d.cards.begin(),d.cards.end(),[](int c){return !C[c].kind;})==22);decksBuilt++;}
 Game g;g.p[0].faction=2;g.p[0].hand={id("Arachas"),id("Arachas"),id("Geralt of Rivia")};g.p[0].deck={id("Cirilla Fiona Elen Riannon"),id("Vesemir")};g.p[1].faction=2;
 auto inv=inventory(g);assert(openingSwap(g,0,2)!=2);auto reversed=g;reverse(reversed.p[0].deck.begin(),reversed.p[0].deck.end());assert(openingSwap(g,0,2)==openingSwap(reversed,0,2));
 reversed.p[1].hand={-999};reversed.p[1].deck={-999};assert(openingSwap(g,0,2)==openingSwap(reversed,0,2));strategicMulligan(g,0);assert(inventory(g)==inv);
 // Keeping both copies of a summon wastes a draw; bond partners are useful.
 int a=id("Arachas"),geralt=id("Geralt of Rivia"),bond=id("Clan Drummond Shield Maiden");
 assert(openingHandValue({a,geralt},{a},2,0,10)>openingHandValue({a,a},{geralt},2,0,10));
 assert(openingHandValue({bond,bond},{},4,0,20)>2*openingHandValue({bond},{},4,0,20));
 auto start=chrono::steady_clock::now();
 for(int f=0;f<5;f++)for(int enemy=0;enemy<5;enemy++){
  begin(1234+f*5+enemy,enemy,f,factionLeaders[enemy][0],-1,true,0,{});
  validateDeck(decks[1]);assert(game.p[1].hand.size()==size_t(10+(game.p[1].leader==15&&!game.p[1].used)));
  map<int,int> expected,actual;for(int c:decks[1].cards)expected[c]++;for(int c:game.p[1].hand)actual[c]++;for(int c:game.p[1].deck)actual[c]++;assert(actual==expected);
 }
 cout<<"25 production preparations ms="<<chrono::duration<double,milli>(chrono::steady_clock::now()-start).count()<<"\n";
 cout<<"PASS: "<<decksBuilt<<" legal strategy/leader/matchup builds, opening swap order/hidden-state invariance and inventory\n";
}
