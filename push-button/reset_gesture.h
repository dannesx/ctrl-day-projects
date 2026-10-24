#pragma once
struct ResetGesture {
  bool holding=false, locked=false;
  unsigned long started=0;
  bool update(unsigned long now,bool yellowDown,bool blueDown,bool playing){
    if(!yellowDown && !blueDown)locked=false;
    if(!(playing && yellowDown && blueDown)){holding=false;return false;}
    if(!holding){holding=true;started=now;}
    if(!locked && ((now-started)&0xffffffffUL)>=3000){locked=true;return true;}
    return false;
  }
};
