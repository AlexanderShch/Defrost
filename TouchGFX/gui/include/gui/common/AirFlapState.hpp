#ifndef AIRFLAPSTATE_HPP
#define AIRFLAPSTATE_HPP

enum class AirFlapState : unsigned char
{
    Moving = 0, // команда и концевик не совпадают (ход)
    Open,       // Air_Open=0, заслонка открыта
    Closed,     // Air_Open=1 при команде закрыть
    Invalid
};

/** Air_Open=0 — открыта, вентилятор продувки можно включать.
 *  Air_Open=1 — закрыта или ещё открывается, вентилятор включать нельзя. */
inline AirFlapState ResolveAirFlapState(unsigned char waterFlapOpen, unsigned char airOpen)
{
    if (waterFlapOpen != 0u)
    {
        if (airOpen == 0u)
            return AirFlapState::Open;
        return AirFlapState::Moving;
    }
    if (airOpen != 0u)
        return AirFlapState::Closed;
    return AirFlapState::Moving;
}

#endif // AIRFLAPSTATE_HPP
