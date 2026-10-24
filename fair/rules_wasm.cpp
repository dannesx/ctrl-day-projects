#include "../tic-tac-toe/training_timer.h"
#include "../push-button/reset_gesture.h"
static ResetGesture gesture;
extern "C" {
unsigned remaining(int ready,unsigned completed,unsigned now){return trainingRemaining(ready,completed,now);}
int reset(unsigned now,int yellow,int blue,int playing){return gesture.update(now,yellow,blue,playing);}
}
