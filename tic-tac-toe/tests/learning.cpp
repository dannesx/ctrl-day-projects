#include "learning.h"
#include <cstdio>
#include <cassert>
#include <initializer_list>
int main(){for(unsigned seed: {1U,42U,2026U}) {Learner l;l.reset(seed);for(unsigned i=0;i<l.EPISODES;++i)l.episode();int wins=0,loss=0,draw=0;
for(int n=0;n<10000;++n){char b[10]="---------";char who=n%2?'O':'X';while(l.outcome(b)=='-'){int a;if(who=='O')a=l.choose(b,who);else{int c[9],cnt=0;for(int i=0;i<9;++i)if(b[i]=='-')c[cnt++]=i;a=c[l.random()%cnt];}assert(a>=0&&b[a]=='-');b[a]=who;who=who=='X'?'O':'X';}char r=l.outcome(b);wins+=r=='O';loss+=r=='X';draw+=r=='D';}printf("wins=%d losses=%d draws=%d\n",wins,loss,draw);assert(loss<100);}}
