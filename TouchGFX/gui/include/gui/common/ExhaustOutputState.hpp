#ifndef EXHAUSTOUTPUTSTATE_HPP
#define EXHAUSTOUTPUTSTATE_HPP

inline bool IsExhaustFanOn(unsigned char outSignal)
{
    return outSignal != 0u;
}

#endif // EXHAUSTOUTPUTSTATE_HPP
