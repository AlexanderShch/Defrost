#include "PlateauControl.hpp"
#include "DefrostControl.h"
#include "ServerTxSize.h"
#include <stddef.h>
static_assert(sizeof(ControlLogPayload_t)==105,"Extended log payload");
static_assert(offsetof(ControlLogPayload_t,pidIntegral)==89,"Legacy prefix must not move");
static_assert(4+sizeof(ControlLogPayload_t)+2<=SERVER_TX_ITEM_SIZE-2,"Extended frame fits queue");
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
extern "C" int test_main() {
 ControlLogPayload_t wire={};wire.pidIntegral=1.25f;wire.pidDerivative=-.5f;wire.pidFeedforward=.25f;wire.pidWaterDuty=.125f;
 const uint8_t* bytes=(const uint8_t*)&wire;
 CHECK(bytes[89]==0&&bytes[90]==0&&bytes[91]==0xa0&&bytes[92]==0x3f);
 CHECK(bytes[93]==0&&bytes[94]==0&&bytes[95]==0&&bytes[96]==0xbf);
 PlateauParams p=Plateau::Defaults();CHECK(Plateau::Valid(p));
 PlateauParams bad=p;bad.tempKi=NAN;CHECK(!Plateau::Valid(bad));
 bad=p;bad.rateWindow_s=2.5f;CHECK(!Plateau::Valid(bad));
 bad=p;bad.pulseMin_s=9;bad.pulseMax_s=5;CHECK(!Plateau::Valid(bad));
 // Entry and live gain changes preserve the command for the same measurement.
 Plateau::Controller c;Plateau::Result r=c.Step(p,23.5f,24,80,80,true,1,1.2f);
 CHECK(fabsf(r.heat-1.2f)<1e-5f);
 float before=r.heat;p.tempKi=.002f;p.tempKp=.25f;
 r=c.Step(p,23.5f,24,80,80,true,1,before);CHECK(fabsf(r.heat-before)<1e-5f);
 // Heating derivative is negative, cooling derivative is positive.
 p=Plateau::Defaults();c.Reset();
 for(int n=0;n<120;++n)r=c.Step(p,23+n*.01f,24,80,80,true,1,1);
 CHECK(r.derivative<-.5f);
 for(int n=0;n<120;++n)r=c.Step(p,24.2f-n*.01f,24,80,80,true,1,1);
 CHECK(r.derivative>.5f);
 // Temperature setpoint step is not differentiated.
 c.Reset();for(int n=0;n<80;++n)r=c.Step(p,24,24,80,80,true,1,1);
 r=c.Step(p,24,25,80,80,true,1,1);CHECK(fabsf(r.derivative)<1e-6f);
 // Saturation includes FF; prolonged high demand cannot wind up without limit.
 c.Reset();for(int n=0;n<2000;++n)r=c.Step(p,20,24,60,80,true,1,1);
 CHECK(r.heat<=2&&r.integral<2);CHECK(r.waterDuty<=p.humidityDutyMax+1e-6f);
 // Water scheduling depends only on humidity; temperature cannot postpone water.
 Plateau::Controller hot,cold;bool hadWater=false;int run=0,pause=600,lastRun=0;
 for(int n=0;n<2000;++n) {
  auto a=hot.Step(p,25,24,65,80,true,1,1);
  auto b=cold.Step(p,22,24,65,80,true,1,1);
  CHECK(a.water==b.water);CHECK(fabsf(a.waterDuty-b.waterDuty)<1e-6f);
  CHECK(b.feedforward>=0&&b.feedforward<=p.ffMaxEq+1e-6f);
  CHECK(a.feedforward==0);CHECK(a.heat>=0&&a.heat<=2);
  if(a.water) {if(run==0)CHECK(pause>=p.pauseMin_s);++run;pause=0;hadWater=true;CHECK(run<=p.pulseMax_s);}
  else {if(run)lastRun=run;run=0;++pause;}
 }
 CHECK(hadWater&&lastRun>0);
 // Loss of humidity/exhaust veto terminates injection immediately.
 r=cold.Step(p,22,24,65,80,false,1,1);CHECK(!r.water&&r.waterDuty==0);
 r=hot.Step(p,25,24,90,80,true,1,1);CHECK(!r.water&&r.waterDuty==0);
 r=cold.Step(p,22,24,-3276.8f,80,true,1,1);CHECK(!r.water&&r.waterDuty==0);
 r=cold.Step(p,22,24,80,80,true,1,1);CHECK(!r.water&&r.waterDuty==0);
 // Raising configured pulse maximum cannot extend an active dose.
 c.Reset();p.pulseMax_s=4;p.pauseMin_s=1;p.pulseMin_s=4;p.humidityDutyMax=.5f;
 int n=0;do{r=c.Step(p,22,24,50,80,true,1,1);}while(!r.water&&++n<100);
 CHECK(r.water);p.pulseMax_s=20;
 int remaining=0;do{r=c.Step(p,22,24,50,80,true,1,1);if(r.water)++remaining;}while(r.water&&remaining<30);
 CHECK(remaining<=3);
 // Raising heatScale after a protected interval has not accumulated temperature I.
 p=Plateau::Defaults();c.Reset();r=c.Step(p,23,24,80,80,true,0,1);
 float protectedI=r.integral;
 for(int q=0;q<1000;++q)r=c.Step(p,23,24,80,80,true,0,1);
 CHECK(fabsf(r.integral-protectedI)<1e-5f);
 return 0;
}
