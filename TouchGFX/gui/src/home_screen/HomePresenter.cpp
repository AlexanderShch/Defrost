#include <gui/home_screen/HomeView.hpp>
#include <gui/home_screen/HomePresenter.hpp>
#include <gui/model/Model.hpp>
#include "DefrostControl.h"

HomePresenter::HomePresenter(HomeView& v)
    : view(v),
      lastRuntimeSeconds(0)
{

}

void HomePresenter::activate()
{
    lastRuntimeSeconds = 0;
    view.updateProgramRuntimeView(0);
    view.updateAlarmBanner(Model::DFR._Alr != 0);
}

void HomePresenter::deactivate()
{

}
void HomePresenter::ValUpdatePresenter()
{
//	 	0 - defroster left
//		1 - defroster right
//	  	2 - defroster center
//	 	3 - fish left
//	 	4 - fish right
	for (int sensNum = 0; sensNum < SQ; ++sensNum)
	{
		if (Model::getFlagCurrentVal_T_Chng(sensNum) == 1) {
			int Val = Model::getCurrentVal_T(sensNum);
			Model::clearFlagCurrentVal_T_Chng(sensNum);
			switch (sensNum) {
				case 0:								//	 	0 - defroster left temperature check
//					view.Val_T_0UpdateView(Val);
					break;
				case 1:								//	 	1 - defroster right temperature check
//					view.Val_T_1UpdateView(Val);
					break;
				case 2:								//	 	2 - defroster center temperature check
//					view.Val_T_2UpdateView(Val);
					break;
				case 3:								//	 	3 - fish left temperature check
					view.Val_T_3UpdateView(Val);
					break;
				case 4:								//	 	4 - fish right temperature check
					view.Val_T_4UpdateView(Val);
					break;
				default:
					break;
			}
		}
	}
	// Обновляем продуктовые датчики каждый тик,
	// чтобы маркер "--" появлялся/исчезал сразу при смене активности.
	view.Val_T_3UpdateView(Model::getCurrentVal_T(3));
	view.Val_T_4UpdateView(Model::getCurrentVal_T(4));

    const uint32_t runtimeSeconds = DefrostControl_GetRuntimeSeconds();
    if (runtimeSeconds != lastRuntimeSeconds)
    {
        lastRuntimeSeconds = runtimeSeconds;
        view.updateProgramRuntimeView(runtimeSeconds);
    }

    // Синхронизация кнопки ПУСК/СТОП при изменении состояния по команде с сервера
    view.syncStartButtonState();

    // Аварийное сообщение на главном экране (лампа/бит _Alr в регистре DFR)
    view.updateAlarmBanner(Model::DFR._Alr != 0);
}

void HomePresenter::startDefrostRequested()
{
	DefrostControl_SetEnabled(1);
    lastRuntimeSeconds = 0;
    view.updateProgramRuntimeView(0);
}

void HomePresenter::stopDefrostRequested()
{
	DefrostControl_SetEnabled(0);
    lastRuntimeSeconds = 0;
    view.updateProgramRuntimeView(0);
}
