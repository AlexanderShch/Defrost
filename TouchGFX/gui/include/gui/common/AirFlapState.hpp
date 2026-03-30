#ifndef AIRFLAPSTATE_HPP
#define AIRFLAPSTATE_HPP

enum class AirFlapState : unsigned char
{
    Moving = 0, // Air_Open=0, Air_Close=0
    Open,       // Air_Open=1, Air_Close=0
    Closed,     // Air_Open=0, Air_Close=1
    Invalid     // Air_Open=1, Air_Close=1
};

inline AirFlapState ResolveAirFlapState(unsigned char airOpen, unsigned char airClose)
{
    if (airOpen != 0u && airClose == 0u)
    {
        return AirFlapState::Open;
    }
    if (airOpen == 0u && airClose != 0u)
    {
        return AirFlapState::Closed;
    }
    if (airOpen == 0u && airClose == 0u)
    {
        return AirFlapState::Moving;
    }
    return AirFlapState::Invalid;
}

#endif // AIRFLAPSTATE_HPP
