#ifndef AIRFLAPSTATE_HPP
#define AIRFLAPSTATE_HPP

enum class AirFlapState : unsigned char
{
    Moving = 0, // не рисуется: картинка «не определено» (точка) больше не используется
    Open,       // Air_Close=0, канал открыт
    Closed,     // Air_Close=1, перегородка: закрыта или ещё открывается
    Invalid
};

/** Картинка заслонки следует концевику, а не рассогласованию с командой.
 *  Air_Close=0 — открыта. Air_Close=1 — перегородка (закрыта или ход на открытие).
 *  Картинка с точкой (Flap.png) на экран не выводится. */
inline AirFlapState ResolveAirFlapState(unsigned char waterFlapOpen, unsigned char airClose)
{
    (void)waterFlapOpen;
    if (airClose == 0u)
        return AirFlapState::Open;
    return AirFlapState::Closed;
}

#endif // AIRFLAPSTATE_HPP
