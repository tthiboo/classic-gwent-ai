#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
void verify(){
 tests();
 int ger=id("Geralt of Rivia"),cat=id("Catapult"),bers=id("Berserker"),yb=id("Young Berserker"),maid=id("Clan Drummond Shield Maiden");
 Game g;g.p[0].faction=2;g.p[1].faction=3;g.p[0].passed=true;g.turn=1;apply(g,{3});assert(g.p[0].wins==1&&g.p[1].wins==1);g.p[0].passed=true;g.turn=1;apply(g,{3});assert(g.winner==2);
 g=Game();g.p[0].faction=1;g.p[1].faction=0;g.p[0].passed=true;g.turn=1;apply(g,{3});assert(g.p[0].wins==1&&g.p[1].wins==0);
 g=Game();g.p[0].faction=2;g.p[0].board={{ger,0},{cat,2}};g.p[1].passed=true;auto inv=inventory(g);apply(g,{3});assert(g.p[0].board.size()==1&&g.p[0].board[0].id==cat&&inventory(g)==inv);
 g=Game();g.p[0].hand={id("Arachas")};g.p[0].deck={id("Arachas"),id("Arachas Behemoth")};apply(g,{0,id("Arachas"),-1,0});assert(g.p[0].board.size()==2&&g.p[0].deck==vector<int>{id("Arachas Behemoth")});
 g=Game();g.p[0].hand={id("Arachas Behemoth"),id("Arachas")};g.p[0].deck={id("Arachas"),id("Arachas")};inv=inventory(g);apply(g,{0,id("Arachas Behemoth"),-1,2});assert(g.p[0].board.size()==4&&inventory(g)==inv);
 g=Game();g.p[0].hand={id("Cerys"),maid};g.p[0].deck={maid,maid};inv=inventory(g);apply(g,{0,id("Cerys"),-1,0});assert(score(g,0)==46&&inventory(g)==inv);
 g=Game();g.p[0].board={{bers,0},{yb,1}};g.p[0].hand={MARD};inv=inventory(g);apply(g,{0,MARD,-1,0});assert(g.p[0].board[0].id==id("Transformed Vildkaarl")&&g.p[0].board[1].id==yb&&inventory(g)==inv);
 g=Game();g.p[0].board={{yb,1},{yb,1},{yb,1}};g.p[0].hand={id("Ermion")};inv=inventory(g);apply(g,{0,id("Ermion"),-1,1});assert(score(g,0)==80&&inventory(g)==inv);
 g=Game();g.p[0].leader=21;g.p[0].board={{id("Crinfrid Reavers Dragon Hunter"),1},{ger,0}};g.weather={{1,id("Impenetrable Fog")}};assert(score(g,0)==18);
 g=Game();g.p[0].leader=20;g.p[0].grave={id("Thaler")};g.p[1].grave={cat};inv=inventory(g);apply(g,{1});assert(g.p[0].grave.empty()&&g.p[1].grave.empty()&&g.p[0].deck.size()==1&&inventory(g)==inv);
 g=Game();g.p[0].leader=12;g.p[0].hand={cat,ger};g.p[0].deck={id("Thaler")};inv=inventory(g);apply(g,{1});assert(g.pendingKind==1);apply(g,{4,cat});apply(g,{4,ger});assert(g.pendingKind==2);apply(g,{5,id("Thaler")});assert(g.pending==-1&&g.turn==1&&inventory(g)==inv);
 g=Game();g.p[0].faction=4;g.p[0].wins=1;g.round=1;g.p[0].grave={cat,ger,id("War Longship")};g.p[1].board={{ger,0}};g.p[1].passed=true;inv=inventory(g);apply(g,{3});assert(g.round==2&&g.p[0].board.size()==2&&g.p[0].grave==vector<int>{ger}&&inventory(g)==inv);
 g=Game();g.p[0].board={{id("Kambi"),0}};g.p[1].passed=true;inv=inventory(g);apply(g,{3});assert(g.p[0].board[0].id==HEMDALL&&inventory(g)==inv);
 g=Game();g.p[0].leader=8;g.p[1].hand={cat,ger,id("Thaler"),id("Dandelion")};g.p[1].known={cat};apply(g,{1});assert(count(g.p[1].known.begin(),g.p[1].known.end(),cat)==1);for(int c:g.p[1].known)assert(count(g.p[1].known.begin(),g.p[1].known.end(),c)<=count(g.p[1].hand.begin(),g.p[1].hand.end(),c));
 for(int f=0;f<5;f++)for(int l:factionLeaders[f])validateDeck(preset(f,l));
 cout<<"Mechanics: ties, Nilfgaard either seat, monster carryover, directional muster, Cerys, Mardroeme, Bran, Crach, Eredin, Skellige round 3, Kambi, all 22 leader presets passed"<<endl;
}
int main(){setup();initClassic();originals=loadDecks("decks.txt");verify();int matches=0,checks=0;
 for(int f=0;f<5;f++)for(int o=0;o<5;o++){
  auto n=preset(f,factionLeaders[f][0]),ng=preset(o,factionLeaders[o][0]);auto g=start(n,ng,811009+f*317+o*43);auto inv=inventory(g);Rng r(1109+f*3+o);vector<Deck> lib={n,ng};
  while(g.winner<0){assert(g.steps<500);int s=g.turn;auto own=s==0?n:ng;
   auto a=playV8(g,own,lib,r.next(),false).chosen;auto legalMoves=legal(g);assert(find(legalMoves.begin(),legalMoves.end(),a)!=legalMoves.end());
   if(g.steps==0){auto poisoned=g;fill(poisoned.p[1-s].hand.begin(),poisoned.p[1-s].hand.end(),-998);fill(poisoned.p[1-s].deck.begin(),poisoned.p[1-s].deck.end(),-999);reverse(poisoned.p[s].deck.begin(),poisoned.p[s].deck.end());assert(playV8(g,own,lib,99,false).chosen==playV8(poisoned,own,lib,99,false).chosen);checks++;}
   apply(g,a);assert(inventory(g)==inv);
  }
  cout<<"match "<<f<<" vs "<<o<<" steps="<<g.steps<<" winner="<<g.winner<<endl;matches++;
 }
 cout<<"Complete matches="<<matches<<" hidden information checks="<<checks<<endl;
}
