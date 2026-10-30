#ifndef AIRFLAPSTATE_HPP
#define AIRFLAPSTATE_HPP

enum class AirFlapState : unsigned char
{
    Moving = 0, // команда и концевик не совпадают (ход)
    Open,       // Air_Open=1
    Closed,     // Air_Open=0
    Invalid
};

/** Положение заслонки: Air_Open 1=открыта / 0=закрыта; Moving — пока DI не совпал с Water_Flap. */
inline AirFlapState ResolveAirFlapState(unsigned char waterFlapOpen, unsigned char airOpen)
{
    if (waterFlapOpen != 0u)
    {
        if (airOpen != 0u)
            return AirFlapState::Open;
        return AirFlapState::Moving;
    }
    if (airOpen == 0u)
        return AirFlapState::Closed;
    return AirFlapState::Moving;
}

#endif // AIRFLAPSTATE_HPP
