#pragma once
#include <ArduinoJson.h>
#include <vector>

// JSON lines only for protocol; human diagnostic lines are ignored by the host.
struct FairResult { uint32_t id; String fields; };
std::vector<FairResult> fairPending;
String fairBoot, fairDevice, fairInput;
uint32_t fairSent = 0;
size_t fairResend = 0;
void fairCommand(JsonDocument& command);
String fairState();
const char* fairProject();
void fairEmit(const char* type, uint32_t game, const String& fields = "") {
  Serial.println(String("{\"v\":1,\"project\":\"") + fairProject() + "\",\"device\":\"" + fairDevice +
    "\",\"boot\":\"" + fairBoot + "\",\"game\":" + game + ",\"type\":\"" + type + "\"" + fields + "}");
}
void fairBegin() {
  fairDevice = String((unsigned long long)ESP.getEfuseMac(), HEX);
  fairBoot = String(esp_random(), HEX) + String(esp_random(), HEX);
  fairEmit("hello", 0);
}
void fairResult(uint32_t id, const String& fields) {
  fairPending.push_back({id, fields});
  fairEmit("result", id, fields);
}
void fairLoop(uint32_t game) {
  // Bound input processing as well as input size to keep games responsive.
  for (int n=0; n<256 && Serial.available(); ++n) {
    char c = Serial.read();
    if(c=='\n') {
      JsonDocument doc;
      if (!deserializeJson(doc, fairInput) && doc["v"] == 1) {
        String type = doc["type"] | "";
        if (type=="hello") fairEmit("hello", game);
        else if (type=="ack" && doc["boot"] == fairBoot && doc["device"] == fairDevice) {
          uint32_t id=doc["game"];
          for(auto it=fairPending.begin();it!=fairPending.end();++it) if(it->id==id){fairPending.erase(it);break;}
        } else if (doc["boot"] == fairBoot && doc["device"] == fairDevice) fairCommand(doc);
      }
      fairInput="";
    } else if(c!='\r') {
      if(fairInput.length()<1024) fairInput+=c;
      else fairInput="INVALID";
    }
  }
  if(millis()-fairSent>=500) {
    fairSent=millis(); fairEmit("state", game, fairState());
    // One pending result per tick keeps a long offline backlog from blocking play.
    if(!fairPending.empty()) {
      fairResend%=fairPending.size();
      auto& item=fairPending[fairResend++];fairEmit("result",item.id,item.fields);
    }
  }
}
