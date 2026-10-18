// Host simulation of the actual sketch, with an absent or responding sensor.
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>
using byte = uint8_t;
constexpr int LOW=0, HIGH=1, OUTPUT=1, INPUT=0;
unsigned long nowUs=0;
bool echoEnabled=false;
unsigned long echoStart=0;
std::vector<unsigned long> steps;
void pinMode(int,int) {}
void digitalWrite(int pin,int value) {
  if (pin==5) steps.push_back(nowUs);
  if (pin==6 && value==LOW) echoStart=nowUs+200;
}
int digitalRead(int) {
  return echoEnabled && nowUs>=echoStart && nowUs<echoStart+1200;
}
unsigned long micros() { return nowUs; }
struct SerialMock {
  char command=0;
  int lines=0;
  void begin(int) {}
  int available() { return command!=0; }
  char read() { char c=command; command=0; return c; }
  int availableForWrite() { return 64; }
  template<class T> void print(T) {}
  template<class T> void print(T,int) {}
  template<class T> void println(T) { ++lines; }
  template<class T> void println(T,int) { ++lines; }
} Serial;
#include "../sonar.ino"
int main() {
  setup();
  for (bool echo : {false,true}) {
    echoEnabled=echo;
    steps.clear();
    Serial.command='S';
    int before=Serial.lines;
    unsigned long end=nowUs+10000000;
    for (; nowUs<end; nowUs+=10) {
      loop();
      assert(posicao>=0 && posicao<=1024);
    }
    assert(steps.size()>3300);
    for (size_t i=1;i<steps.size();++i) {
      assert(steps[i]-steps[i-1]>=INTERVALO_PASSO_US);
      assert(steps[i]-steps[i-1]<=INTERVALO_PASSO_US+10);
    }
    assert(Serial.lines-before>140);
    Serial.command='P'; loop();
    auto count=steps.size();
    for (int i=0;i<10000;++i) { nowUs+=10; loop(); }
    assert(steps.size()==count && estadoSensor==OCIOSO);
    int saved=posicao;
    Serial.command='R'; loop();
    assert(posicao==saved);
    std::cout << (echo ? "Echo" : "Timeout") << ": regular steps, bounds, pause/resume passed\n";
  }
}
