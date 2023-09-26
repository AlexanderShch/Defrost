#include <gui/settings5_screen/Settings5View.hpp>
#include <gui/settings5_screen/Settings5Presenter.hpp>

Settings5Presenter::Settings5Presenter(Settings5View& v)
    : view(v)
{

}

void Settings5Presenter::activate()
{

}

void Settings5Presenter::deactivate()
{

}

void Settings5Presenter::ValUpdatePresenter()
{
	if (Model::getFlagCurrentVal_PR_Chng() == 1)
	{
		Model::clearFlagCurrentVal_PR_Chng();
		view.Val_Addr_UpdateView(Model::getCurrentAddress_PR());
		view.Val_BoadRate_UpdateView(Model::getCurrentBoadRate_PR());
	}
}
