#define CLASSIC_NO_MAIN
#include "classic_bridge.cpp"
#include "verified_review.inc"
uint64_t reviewID(const string& text){uint64_t h=1469598103934665603ULL;for(unsigned char c:text){h^=c;h*=1099511628211ULL;}return h?h:1;}
int main(int argc,char** argv){setup();initClassic();try{
 if(argc!=2)throw runtime_error("isolated lessons path required");bool loaded=verifiedLessons.load(argv[1]);if(filesystem::exists(argv[1])&&!loaded)throw runtime_error("invalid existing lesson memory; preserved");string line;cout.precision(17);
 while(getline(cin,line)){vector<string> fields;istringstream row(line);string v;while(getline(row,v,'\t'))fields.push_back(v);if(fields.size()!=4)throw runtime_error("invalid lesson input");
  auto r=decodeReplay(fields[0]);if(r.observed.turn!=1)throw runtime_error("only bot-observer inputs may train gameplay memory");auto text=fields[1];replace(text.begin(),text.end(),',',' ');istringstream action(text);Act played;if(!(action>>played.kind>>played.card>>played.target>>played.row))throw runtime_error("invalid played action");
  auto oldGeneration=verifiedLessons.generation;auto review=certifyLesson(r,played,stoi(fields[3]));bool accepted=review.complete&&verifiedLessons.observe(r.observed,review.preferred,review.other,reviewID(fields[2]),review.proof);
  if(verifiedLessons.generation!=oldGeneration&&!verifiedLessons.save())throw runtime_error("lesson commit failed");
  cout<<"{\"position\":"<<quote(fields[2])<<",\"complete\":"<<(review.complete?"true":"false")<<",\"accepted\":"<<(accepted?"true":"false")<<",\"nodes\":"<<review.nodes<<",\"preferred\":"<<quote(actionKey(review.preferred))<<",\"other\":"<<quote(actionKey(review.other))<<",\"trials\":"<<review.proof.trials<<",\"gain\":["<<review.proof.mean[0]<<','<<review.proof.mean[1]<<"],\"standardError\":["<<review.proof.standardError[0]<<','<<review.proof.standardError[1]<<"],\"exactConfirmation\":"<<(review.proof.strongConfirmation?"true":"false")<<",\"cases\":"<<verifiedLessons.cases.size()<<"}"<<endl;
 }
 }catch(const exception& e){cerr<<e.what()<<'\n';return 1;}}
