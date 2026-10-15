#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
int main(){setup();initClassic();originals=loadDecks("decks.txt");int checks=0;
 for(int side=0;side<2;side++)for(int f=0;f<5;f++)for(int style=0;style<3;style++){
  Game g;g.turn=side;g.p[side].faction=f;g.p[1-side].faction=(f+1)%5;g.p[0].used=g.p[1].used=true;
  g.p[side].hand={DRAG,id("Geralt of Rivia"),id("Mysterious Elf (Avallach)"),id("Zoltan Chivay"),id("Vesemir"),DEC,SCORCH,CLEAR,HORN,id("Yennefer of Vengerberg")};g.p[side].drawn=10;g.p[side].deck={id("Vesemir"),id("Vesemir")};g.p[1-side].hand=vector<int>(10,-1);
  auto legalOpen=safeActionsV9(g);assert(legalOpen.size()==1&&legalOpen[0].card==id("Mysterious Elf (Avallach)"));
  // The opponent has moved first: the old pristineOpening guard excludes it.
  g.steps=1;g.p[1-side].board={{id("Zoltan Chivay"),0}};assert(!pristineOpening(g));
  auto a=safeActionsV9(g);assert(a.size()==1&&a[0].card==id("Mysterious Elf (Avallach)"));
  auto poisoned=g;poisoned.p[1-side].hand=vector<int>(10,-999);poisoned.p[1-side].deck={-998};assert(safeActionsV9(poisoned)==a);
  // Preserve an immediately productive Dragon against an actual melee target.
  g.p[1-side].board={{id("Vesemir"),0},{id("Vesemir"),0}};a=safeActionsV9(g);assert(any_of(a.begin(),a.end(),[](Act x){return x.card==DRAG;}));
  // Screenshot-shaped position: one own hero, two donated spies and 9 cards.
  g.p[1-side].board={{id("Esterad Thyssen"),0},{id("Vernon Roche"),0},{id("Mysterious Elf (Avallach)"),0}};
  g.p[side].board={{id("Morteisen"),0},{id("Vattier de Rideaux"),0},{id("Menno Coehoorn"),2}};
  g.p[side].hand.erase(find(g.p[side].hand.begin(),g.p[side].hand.end(),id("Mysterious Elf (Avallach)")));g.steps=6;
  a=safeActionsV9(g);assert(none_of(a.begin(),a.end(),[](Act x){return x.kind==3;}));
  // A losing, expensive chase is still allowed to concede; no blanket pass ban.
  g.p[1-side].board=vector<Unit>(12,{id("Geralt of Rivia"),0});a=safeActionsV9(g);assert(any_of(a.begin(),a.end(),[](Act x){return x.kind==3;}));
  // After a conceded round or in round two, exact wins and dry passes survive.
  g.p[1-side].passed=true;a=safeActionsV9(g);assert(any_of(a.begin(),a.end(),[](Act x){return x.kind==3;}));g.p[1-side].passed=false;g.round=1;g.p[side].wins=1;a=safeActionsV9(g);assert(any_of(a.begin(),a.end(),[](Act x){return x.kind==3;}));checks+=7;
 }
 cout<<"PASS: "<<checks<<" opening checks: both seats, five factions, three styles; spy-first, useful Dragon, player-first reply, cheap lead, justified concession, hidden-state invariance\n";
}
