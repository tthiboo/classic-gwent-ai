#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
#include "verified_review.inc"
int main(){setup();initClassic();originals=loadDecks("decks.txt");
 auto own=preset(0,0),enemy=preset(1,3);auto g=start(own,enemy,14101991);g.turn=0;g.round=2;g.p[0].wins=g.p[1].wins=1;g.p[0].used=g.p[1].used=true;
 // Public, deciding-round fixtures: finite full-game minimax is the teacher,
 // not a longer instance of the policy whose guidance previously failed.
 g.p[0].hand={id("Catapult"),id("Trebuchet"),SCORCH,HORN};g.p[1].hand={id("Black Infantry Archer"),id("Torrential Rain")};g.p[0].board={{id("Blue Stripes Commando"),0},{id("Blue Stripes Commando"),0}};g.p[1].board={{id("Black Infantry Archer"),2}};
 auto obs=observation(g,0);auto safe=safeActionsV9(obs);Act a{0,HORN,-1,0},b{0,HORN,-1,1};
 LessonCertificate p;p.mean={1,1};p.standardError={0,0};p.family.fill(1);p.trials=48;p.strongConfirmation=true;
 VerifiedLessons m;assert(m.observe(obs,a,b,8181,p));assert(!m.observe(obs,a,b,8181,p));assert(m.cases.begin()->second.positions==1);
 int matches=0;auto hints=m.hints(obs,safe,&matches);assert(matches>=1);assert(hints[find(safe.begin(),safe.end(),a)-safe.begin()]>hints[find(safe.begin(),safe.end(),b)-safe.begin()]);
 auto poison=obs;poison.p[1].hand.assign(poison.p[1].hand.size(),-999);poison.p[1].deck.assign(poison.p[1].deck.size(),-998);reverse(poison.p[0].deck.begin(),poison.p[0].deck.end());assert(m.hints(poison,safe)==hints);
 auto swapped=obs;swapped.p[0].board={{id("Clan Drummond Shield Maiden"),0},{id("Clan Drummond Shield Maiden"),0}};if(lessonQuery(swapped,a,b).key!=lessonQuery(obs,a,b).key){cerr<<"scores "<<score(obs,0)<<" "<<score(swapped,0)<<"; base "<<C[id("Blue Stripes Commando")].p<<" "<<C[id("Letho of Gulet")].p<<" "<<C[id("Vesemir")].p<<"\n";}assert(lessonQuery(swapped,a,b).key==lessonQuery(obs,a,b).key); // ID/name never enters this weather/horn preference.
 m.path="/tmp/gwent-verified-lessons-tests.txt";assert(m.save());VerifiedLessons loaded;assert(loaded.load(m.path)&&loaded.hints(obs,safe)==hints);remove(m.path.c_str());
 p.mean={-1,-1};p.family.fill(-1);assert(!m.observe(obs,a,b,8181,p));assert(m.hints(obs,safe)==vector<double>(safe.size()));
 VerifiedLessons unsupported;auto bad=p;bad.strongConfirmation=false;assert(!unsupported.observe(obs,a,b,9,bad));bad=p;bad.mean[1]=1;assert(!unsupported.observe(obs,a,b,10,bad));
 VerifiedLessons frozen;frozen.observe(obs,a,b,37,LessonCertificate{{1,1},{0,0},{1,1,1,1,1,1},48,true});auto replay=encodeReplay(g,0,own,{enemy},8123,0,{1,1,1},{1,0,0},true,false,true,nullptr,&frozen);auto decoded=decodeReplay(replay);assert(decoded.lessons.hints(obs,safe)==frozen.hints(obs,safe));assert(decoded.lessons.seen.empty());
 own.cards.clear();enemy.cards.clear();for(int side=0;side<2;side++){auto &d=side?enemy:own;for(auto v:{g.p[side].hand,g.p[side].deck,g.p[side].grave})d.cards.insert(d.cards.end(),v.begin(),v.end());for(auto u:g.p[side].board)d.cards.push_back(u.id);}auto h=compatible(obs,0,own,{enemy});assert(!h.empty());decisionLearning={};auto cold=parallelRoot(obs,h,331,0,{1,1,1},{128,0,18},true,1,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true,nullptr,nullptr);VerifiedLessons empty;auto same=parallelRoot(obs,h,331,0,{1,1,1},{128,0,18},true,3,true,{1,0,0},false,false,true,true,false,false,nullptr,6,false,nullptr,false,true,nullptr,&empty);assert(cold.chosen==same.chosen);for(int i=0;i<int(cold.root.size());i++)assert(cold.root[i].value==same.root[i].value&&cold.root[i].visits==same.root[i].visits);
 cout<<"PASS: independent certificate admission; duplicate/conflict handling; public-only context transfer; exact persistence/replay; cold parallel equivalence\n";
}
