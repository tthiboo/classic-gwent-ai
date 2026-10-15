// Exact counterfactual certification in the deciding round. Ordered sampled
// piles and RNG belong to the hypothetical world, never to the real opponent.
// Unsaturated minimax or stochastic resurrection yields no training label.
struct ExactLessonReview {Act preferred,other;LessonCertificate proof;bool complete=false;int nodes=0;};
ExactLessonReview certifyLesson(const ReplaySnapshot& r,Act played,int demonstration=-1){
 ExactLessonReview result;auto obs=r.observed;if(!publicLessonEndgame(obs))return result;int side=obs.turn;
 auto safe=safeActionsV9(obs);if(safe.size()<2||find(safe.begin(),safe.end(),played)==safe.end())return result;
 auto h=compatible(obs,side,r.own,r.library);if(h.empty())return result;
 vector<double> values(safe.size());Rng discovery(r.seed^0x54398ce19ULL);
 try{
  for(int t=0;t<12;t++){auto world=sampleBelief(obs,side,h,discovery);world.rng=Rng(discovery.next());ExactLessonSolver solver;solver.ownSide=side;
   for(int k=0;k<int(safe.size());k++){auto after=world;apply(after,safe[k]);values[k]+=(side==0?1:-1)*solver.value(after);}result.nodes+=solver.visited;
  }
  int original=find(safe.begin(),safe.end(),played)-safe.begin();int preferred=max_element(values.begin(),values.end())-values.begin(),other=original;
  // A shown public trick is merely a candidate. It must independently defeat
  // the original move; resemblance supplies neither an outcome nor a label.
  if(demonstration>=0&&demonstration<int(C.size()))for(int k=0;k<int(safe.size());k++)if(safe[k].kind==0&&decisionClass(safe[k])==decisionClass(Act{0,demonstration,-1,safe[k].row})&&C[safe[k].card].kind==C[demonstration].kind&&C[safe[k].card].flags==C[demonstration].flags&&C[safe[k].card].weather==C[demonstration].weather&&values[k]>values[original])preferred=k;
  if(preferred==other){other=-1;for(int k=0;k<int(safe.size());k++)if(k!=preferred&&values[k]<values[preferred]&&(other<0||values[k]>values[other]))other=k;}
  if(other<0||values[preferred]<=values[other])return result;
  result.preferred=safe[preferred];result.other=safe[other];array<double,2> sum{},squares{};array<int,6> count{};
  // Separate streams for selection and each confirmation block. Each label
  // is one position; these worlds never masquerade as 48 independent lessons.
  for(int block=0;block<2;block++){Rng confirmation(r.seed^(block?0x832b67140ULL:0x176abcd91ULL));
   for(int trial=0;trial<24;trial++){auto world=sampleBelief(obs,side,h,confirmation);world.rng=Rng(confirmation.next());ExactLessonSolver solver;solver.ownSide=side;
    auto a=world,b=world;apply(a,result.preferred);apply(b,result.other);
    double difference=(side==0?1:-1)*(solver.value(a)-solver.value(b))/2.0;result.nodes+=solver.visited;sum[block]+=difference;squares[block]+=difference*difference;int family=trial%6;result.proof.family[family]+=difference;count[family]++;result.proof.trials++;
   }
   result.proof.mean[block]=sum[block]/24;result.proof.standardError[block]=sqrt(max(0.0,(squares[block]-sum[block]*sum[block]/24)/23)/24);
  }
  for(int k=0;k<6;k++)result.proof.family[k]/=count[k];result.proof.strongConfirmation=true;result.complete=true;
 }catch(Limit&){return result;}
 return result;
}
