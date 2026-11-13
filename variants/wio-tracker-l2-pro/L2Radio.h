#pragma once
#include <helpers/radiolib/CustomSX1262Wrapper.h>

class L2RadioWrapper : public CustomSX1262Wrapper {
  bool txAllowed = false;
  bool transmitting = false;
public:
  using CustomSX1262Wrapper::CustomSX1262Wrapper;
  bool isTxAllowed() const { return txAllowed; }
  void allowTransmit(bool allow) { txAllowed = allow; }
  bool isTransmitting() const { return transmitting; }
  void setTxPower(int8_t power) {
    txAllowed=false;
    if(power>=-9 && power<=22) CustomSX1262Wrapper::setTxPower(power);
  }
  void setParams(float freq, float bw, uint8_t sf, uint8_t cr) override {
    txAllowed = false; // app-originated changes require local confirmation
    idle();
    if (freq >= 863 && freq <= 925) CustomSX1262Wrapper::setParams(freq,bw,sf,cr);
  }
  bool configure(float freq,float bw,uint8_t sf,uint8_t cr,int8_t power) {
    txAllowed = false;
    if (transmitting || freq < 863 || freq > 925 || power < -9 || power > 22) return false;
    idle();
    auto* chip = static_cast<CustomSX1262*>(_radio);
    if (chip->setFrequency(freq) || chip->setBandwidth(bw) || chip->setSpreadingFactor(sf) ||
        chip->setCodingRate(cr) || chip->setOutputPower(power)) return false;
    CustomSX1262Wrapper::setParams(freq,bw,sf,cr);
    return true;
  }
  bool startSendRaw(const uint8_t* bytes, int len) override {
    // Final hardware boundary: includes ACKs, adverts, relays and API commands.
    transmitting = txAllowed && CustomSX1262Wrapper::startSendRaw(bytes, len);
    return transmitting;
  }
  void onSendFinished() override {
    CustomSX1262Wrapper::onSendFinished(); transmitting = false;
  }
};
