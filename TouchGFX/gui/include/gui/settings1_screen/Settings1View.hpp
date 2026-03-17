#ifndef SETTINGS1VIEW_HPP
#define SETTINGS1VIEW_HPP

#include <gui_generated/settings1_screen/Settings1ViewBase.hpp>
#include <gui/settings1_screen/Settings1Presenter.hpp>

#include "ModBus.hpp"
#include <touchgfx/Callback.hpp>

class Settings1View : public Settings1ViewBase
{
public:
    Settings1View();
    virtual ~Settings1View() {}
    virtual void setupScreen() override;
    virtual void tearDownScreen() override;
    virtual void handleTickEvent() override;
    virtual void BTNCoreTSetIncreaseClicked() override;
    virtual void BTNCoreTSetDecreaseClicked() override;

    /** Синхронизировать отображаемую уставку с DefrostControl (при изменении с сервера). */
    void syncCoreTSetFromDefrostControl();

protected:
    // Уставка целевой температуры рыбы в десятых долях градуса (например, 65 = 6.5 °C).
    int16_t CoreTSetDeci = 60; // 6.0 °C по умолчанию

    touchgfx::Callback<Settings1View, const touchgfx::AbstractButton&> coreTSensorToggleCallback;
    void coreTSensorToggleCallbackHandler(const touchgfx::AbstractButton& src);

    // Состояние для автоинкремента/декремента при длительном нажатии.
    bool incPressedPrev = false;
    bool decPressedPrev = false;
    uint16_t incPressTicks = 0;
    uint16_t decPressTicks = 0;
    uint16_t incRepeatTicks = 0;
    uint16_t decRepeatTicks = 0;

    void applyCoreTSetAndRedraw();
    /** Обновить только отображение ValueCoreTSet по текущему CoreTSetDeci (без записи в DefrostControl). */
    void applyCoreTSetDisplayOnly();
    void stepIncreaseOnce();
    void stepDecreaseOnce();
};

#endif // SETTINGS1VIEW_HPP
