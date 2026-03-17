#include <gui/settings1_screen/Settings1View.hpp>
#include <gui/settings1_screen/Settings1Presenter.hpp>
#include "DefrostControl.h"

Settings1Presenter::Settings1Presenter(Settings1View& v)
    : view(v)
{

}

void Settings1Presenter::activate()
{

}

void Settings1Presenter::deactivate()
{

}

void Settings1Presenter::DefrosterOperatingTemperaturePresenter(float Val)
{
	DefrostControl_SetFishColdTarget_C(Val);
}

float Settings1Presenter::GetFishColdTarget_C()
{
	return DefrostControl_GetFishColdTarget_C();
}

void Settings1Presenter::ValUpdatePresenter()
{
	// Синхронизация уставки «конечная температура ядра» при изменении с сервера (CFG_CMD_SET_DEFROST_PARAM).
	view.syncCoreTSetFromDefrostControl();
}
