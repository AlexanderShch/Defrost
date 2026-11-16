#ifndef PLATEAU_CONTROL_HPP
#define PLATEAU_CONTROL_HPP
#include "PlateauParams.h"
#include <math.h>
#include <string.h>
#include <stdint.h>
namespace Plateau {
inline float Bound(float x,float lo,float hi) { return x<lo?lo:(x>hi?hi:x); }
inline PlateauParams Defaults() {
 PlateauParams p;
#define PLATEAU_DEFAULT(name, def, lo, hi) p.name=def;
 PLATEAU_FIELDS(PLATEAU_DEFAULT)
#undef PLATEAU_DEFAULT
 return p;
}
inline bool Valid(const PlateauParams& p) {
#define PLATEAU_CHECK(name, def, lo, hi) if(!isfinite(p.name)||p.name<lo||p.name>hi) return false;
 PLATEAU_FIELDS(PLATEAU_CHECK)
#undef PLATEAU_CHECK
 return p.pulseMin_s<=p.pulseMax_s &&
   floorf(p.rateWindow_s)==p.rateWindow_s && floorf(p.pulseMax_s)==p.pulseMax_s &&
   floorf(p.pulseMin_s)==p.pulseMin_s && floorf(p.pauseMin_s)==p.pauseMin_s;
}
struct Result { float heat, integral, derivative, feedforward, waterDuty; uint8_t water; };
/* One invocation per real 1s control tick; no heap allocation, no IO. */
class Controller {
 PlateauParams prev;
 float history[121], integral, slope, rhFiltered, rhRate, rhIntegral;
 float debt, tailRemaining, lastHeat, lastDuty;
 uint16_t head,count,onSeconds,offSeconds,pulseMaxLatched,pulseMinLatched;
 bool initialized,on,rhInitialized;
public:
 Controller() { Reset(); }
 void Reset() { memset(this,0,sizeof(*this)); offSeconds=600; }
 Result Step(const PlateauParams& p,float temperature,float setpoint,float rh,float rhSet,
             bool waterAllowed,float heatScale,float previousHeat) {
  Result out={};
  if(!Valid(p)||!isfinite(temperature)||!isfinite(setpoint)||!isfinite(heatScale)) { Reset();return out; }
  bool first=!initialized;
  bool changed=!first && memcmp(((const uint8_t*)&prev)+sizeof(float),((const uint8_t*)&p)+sizeof(float),sizeof(p)-sizeof(float))!=0;
  if(first) { rhFiltered=isfinite(rh)?rh:rhSet;lastHeat=Bound(previousHeat,0,2); }
  if(changed && p.rateWindow_s!=prev.rateWindow_s) {count=0;slope=0;}
  unsigned window=(unsigned)p.rateWindow_s;
  float rawRate=0;
  if(count>=window) rawRate=(temperature-history[(head+121-window)%121])/window;
  history[head]=temperature;head=(head+1)%121;if(count<121)++count;
  slope+=(rawRate-slope)/(1+p.rateTau_s);
  float derivative=Bound(-p.tempKd*slope,-p.dLimitEq,p.dLimitEq);
  bool validHumidity=waterAllowed && isfinite(rh)&&isfinite(rhSet)&&rh>=0&&rh<=100&&rhSet>=10&&rhSet<=100;
  float rhError=0,humidityD=0;
  float achievableDuty=p.pulseMax_s/(p.pulseMax_s+p.pauseMin_s);
  float maxDuty=Bound(p.humidityDutyMax,0,achievableDuty);
  if(validHumidity) {
   if(!rhInitialized) {rhFiltered=rh;rhRate=0;rhIntegral=0;rhInitialized=true;}
   float old=rhFiltered;rhFiltered+=(rh-rhFiltered)/(1+p.humidityTau_s);
   rhRate=rhFiltered-old;rhError=rhSet-rhFiltered;
   if(fabsf(rhError)<=p.humidityDeadbandRH)rhError=0;
   humidityD=-p.humidityKd*rhRate;
   if(changed) rhIntegral=lastDuty-p.humidityKp*rhError-humidityD;
   float trial=rhIntegral+p.humidityKi*rhError;
   float wanted=p.humidityKp*rhError+trial+humidityD;
   if(!((wanted>maxDuty&&rhError>0)||(wanted<0&&rhError<0)))rhIntegral=trial;
   out.waterDuty=Bound(p.humidityKp*rhError+rhIntegral+humidityD,0,maxDuty);
   // Do not continue delivering an accumulated dose above the measured upper bound.
   if(rh>=rhSet+p.humidityOverRH) {out.waterDuty=0;rhIntegral=0;}
  } else {rhIntegral=0;rhInitialized=false;out.waterDuty=0;}
  lastDuty=out.waterDuty;
  // Pulse-density scheduler: PID determines water-time debt, hence pulse length and interval.
  // Max pulse and minimum pause are actuator bounds, not a fixed injection schedule.
  if(out.waterDuty<=0) {debt=0;if(on){on=false;offSeconds=0;}onSeconds=0;}
  else {
   debt=Bound(debt+out.waterDuty,-p.pulseMax_s,p.pulseMax_s);
   if(on && (onSeconds>=Bound(p.pulseMax_s,1,(float)pulseMaxLatched) || (onSeconds>=pulseMinLatched&&debt<=0))) {
    on=false;onSeconds=0;offSeconds=0;
   }
   if(!on && offSeconds>=p.pauseMin_s && debt>=p.pulseMin_s) {
    on=true;onSeconds=0;pulseMaxLatched=(uint16_t)p.pulseMax_s;pulseMinLatched=(uint16_t)p.pulseMin_s;
   }
  }
  if(on){++onSeconds;debt-=1;}else if(offSeconds<600)++offSeconds;
  out.water=on?1:0;
  // Temperature does not veto or reschedule water. It only gates heat feedforward.
  float envelope=0;
  if(on) {tailRemaining=p.ffTail_s;envelope=1;}
  else if(tailRemaining>0 && p.ffTail_s>0) {
   tailRemaining=Bound(tailRemaining,0,p.ffTail_s);envelope=tailRemaining/p.ffTail_s;
   tailRemaining=Bound(tailRemaining-1,0,p.ffTail_s);
  } else tailRemaining=0;
  float error=setpoint-temperature;
  float ff=p.ffMaxEq*envelope*Bound((error+p.ffBand_C)/(2*p.ffBand_C),0,1);
  if(first||changed) integral=lastHeat-p.tempKp*error-derivative-ff;
  else {
   float trial=integral+p.tempKi*error;
   float wanted=p.tempKp*error+trial+derivative+ff;
   if(!((wanted>2&&error>0)||(wanted<0&&error<0)||(heatScale<.999f&&error>0)))integral=trial;
  }
  out.heat=Bound(p.tempKp*error+integral+derivative+ff,0,2);
  out.integral=integral;out.derivative=derivative;out.feedforward=ff;
  lastHeat=out.heat;prev=p;initialized=true;
  return out;
 }
};
static_assert(sizeof(PlateauParams)==PLATEAU_PARAM_BYTES,"Plateau wire size");
}
#endif
