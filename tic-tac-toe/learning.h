#pragma once
#include <stdint.h>
#include <string.h>

// Tabular Q-learning over afterstates: a state/action pair is represented by
// the board after that move, with the mover encoded as 1 and opponent as 2.
// Equivalent afterstates share a value. No minimax or tactical rules at runtime.
struct Learner {
  static constexpr int STATES = 19683;
  static constexpr uint32_t EPISODES = 200000;
  int16_t q[STATES] = {};
  uint32_t episodes = 0;
  uint32_t rng = 1;
  uint32_t random() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
  int key(const char* b, char who, int action) const {
    int code=0, power=1;
    for(int i=0;i<9;++i,power*=3) code += power*(i==action ? 1 : b[i]=='-' ? 0 : b[i]==who ? 1 : 2);
    return code;
  }
  char outcome(const char* b) const {
    static const uint8_t lines[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for(auto &l:lines) if(b[l[0]]!='-' && b[l[0]]==b[l[1]] && b[l[1]]==b[l[2]])return b[l[0]];
    for(int i=0;i<9;++i)if(b[i]=='-')return '-';
    return 'D';
  }
  int choose(const char* b, char who, bool explore=false) {
    int cells[9],count=0,best=-1,value=-2000;
    for(int i=0;i<9;++i)if(b[i]=='-') {
      cells[count++]=i;
      int v=q[key(b,who,i)];
      if(v>value){value=v;best=i;}
    }
    if(!count)return -1;
    // Exploration decays from 70% to 15%; inference is always greedy.
    uint32_t epsilon=70-(55*episodes/EPISODES);
    return explore && random()%100<epsilon ? cells[random()%count] : best;
  }
  void episode() {
    char b[10]="---------",who='X';
    // Both perspectives learn; the actual game's starting order is independent.
    int keys[9],targets[9],length=0;
    while(true){
      int a=choose(b,who,true),k=key(b,who,a);
      b[a]=who; char end=outcome(b);
      keys[length]=k;
      if(end!='-') {targets[length++]=end=='D'?0:1000;break;}
      char other=who=='X'?'O':'X';
      int next=choose(b,other);
      targets[length++]=-q[key(b,other,next)]*98/100;
      who=other;
    }
    for(int i=length-1;i>=0;--i) q[keys[i]] += (targets[i]-q[keys[i]])/4;
    ++episodes;
  }
  void reset(uint32_t seed){memset(q,0,sizeof(q));episodes=0;rng=seed?seed:1;}
};
