#ifndef PLATEAU_PARAMS_H
#define PLATEAU_PARAMS_H
/* Wire group 7: 20 float32 slots; slot 0 is reserved and never selects an algorithm. */
#define PLATEAU_FIELDS(X) \
 X(reservedAlgorithm, 1.0f, 0.0f, 1.0f) \
 X(tempKp, .18f, 0.0f, 2.0f) \
 X(tempKi, .003f, 0.0f, .05f) \
 X(tempKd, 60.0f, 0.0f, 120.0f) \
 X(rateWindow_s, 30.0f, 1.0f, 120.0f) \
 X(rateTau_s, 10.0f, 1.0f, 120.0f) \
 X(dLimitEq, .6f, 0.0f, 1.0f) \
 X(ffMaxEq, .3f, 0.0f, 1.0f) \
 X(ffTail_s, 15.0f, 0.0f, 60.0f) \
 X(ffBand_C, .2f, .05f, 2.0f) \
 X(humidityKp, .02f, 0.0f, 1.0f) \
 X(humidityKi, .0002f, 0.0f, .01f) \
 X(humidityKd, 0.0f, 0.0f, 10.0f) \
 X(humidityTau_s, 10.0f, 1.0f, 120.0f) \
 X(pulseMax_s, 15.0f, 1.0f, 60.0f) \
 X(pauseMin_s, 90.0f, 1.0f, 600.0f) \
 X(pulseMin_s, 1.0f, 1.0f, 10.0f) \
 X(humidityDutyMax, .14285714f, 0.0f, .5f) \
 X(humidityDeadbandRH, 1.0f, 0.0f, 5.0f) \
 X(humidityOverRH, 1.0f, 0.0f, 5.0f)
typedef struct {
#define PLATEAU_MEMBER(name, def, lo, hi) float name;
 PLATEAU_FIELDS(PLATEAU_MEMBER)
#undef PLATEAU_MEMBER
} PlateauParams;
#define PLATEAU_PARAM_COUNT 20u
#define PLATEAU_PARAM_BYTES 80u
#endif
