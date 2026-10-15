// Recent revealed cards only. The selected leader of a new opponent is not
// an input. Exponential forgetting permits a change of strategy across games.
struct PreparationHistory {
 array<double,10> features{};int observations=0;
 void observe(int c,int copies=1){if(c<0||c>=int(C.size())||copies<1)return;auto x=C[c];
  for(int n=0;n<copies;n++){for(double& v:features)v*=.97;features[3]++;
   if(!x.kind&&!(x.flags&(HERO|SPY)))features[x.row==3?1:x.row]+=max(1,x.p);
   features[4]+=!!(x.flags&HERO);features[5]+=!!(x.flags&SPY);features[6]+=!!(x.flags&MED);
   features[7]+=!!x.weather;features[8]+=c==SCORCH||!!(x.flags&(DRAGON|RSCORCH|SSCORCH|GSCORCH));
   features[9]+=!!(x.flags&(BOND|MUSTER));observations=min(100000,observations+1);
  }
 }
};
