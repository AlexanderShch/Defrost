#ifndef AIRFLAPSTATE_HPP
#define AIRFLAPSTATE_HPP

enum class AirFlapState : unsigned char
{
    Moving = 0, // Air_Open_Close=0 — ход заслонки
    Open,       // Water_Flap=1 и Air_Open_Close=1
    Closed,     // Water_Flap=0 и Air_Open_Close=1
    Invalid     // зарезервировано (один концевик не даёт противоречия Open+Close)
};

/** Положение заслонки: конец хода по Air_Open_Close, открыто/закрыто — по команде Water_Flap. */
inline AirFlapState ResolveAirFlapState(unsigned char waterFlapOpen, unsigned char airOpenClose)
{
    if (airOpenClose == 0u)
    {
        return AirFlapState::Moving;
    }
    if (waterFlapOpen != 0u)
    {
        return AirFlapState::Open;
    }
    return AirFlapState::Closed;
}

#endif // AIRFLAPSTATE_HPP
