#ifndef HOMEVIEW_HPP
#define HOMEVIEW_HPP

#include <gui_generated/home_screen/HomeViewBase.hpp>
#include <gui/home_screen/HomePresenter.hpp>
#include <touchgfx/widgets/AbstractButton.hpp>
#include <stdint.h>

class HomeView : public HomeViewBase
{
public:
    HomeView();
    virtual ~HomeView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    /** Повторная привязка действия к BTNStart (вызывать при активации экрана, чтобы колбэк гарантированно был установлен). */
    void bindStartButtonAction();

    virtual void Val_T_3UpdateView(int val);
    virtual void Val_T_4UpdateView(int val);
    void updateProgramRuntimeView(uint32_t runtimeSeconds);

    void updateVersionDisplay();

protected:
    /** Обработчик нажатия кнопки «Старт» — запуск алгоритма разморозки. */
    void onBTNStartClicked(const touchgfx::AbstractButton& src);

    /** Callback для кнопки BTNStart. */
    touchgfx::Callback<HomeView, const touchgfx::AbstractButton&> startButtonCallback;

    static const uint16_t PRGTIMEWRK_SIZE = 9;
    touchgfx::Unicode::UnicodeChar ValuePRGTimeWrkBuffer[PRGTIMEWRK_SIZE];
};

#endif // HOMEVIEW_HPP
