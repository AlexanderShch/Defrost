#include <gui/settings1_screen/Settings1View.hpp>
#include <gui/settings1_screen/Settings1Presenter.hpp>

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
	Model::setCurrentVal_T(SensNum_DefrOperTemp, Val);
}
