#include <gui/home_screen/HomeView.hpp>
#include <gui/home_screen/HomePresenter.hpp>

HomePresenter::HomePresenter(HomeView& v)
    : view(v)
{

}

void HomePresenter::activate()
{

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
}
