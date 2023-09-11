#include <gui/visualization_screen/VisualizationView.hpp>
#include <gui/visualization_screen/VisualizationPresenter.hpp>

VisualizationPresenter::VisualizationPresenter(VisualizationView& v)
    : view(v)
{

}

void VisualizationPresenter::activate()
{

}

void VisualizationPresenter::deactivate()
{

}

void VisualizationPresenter::ValUpdatePresenter()
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
					view.Val_T_0UpdateView(Val);
					break;
				case 1:								//	 	1 - defroster right temperature check
					view.Val_T_1UpdateView(Val);
					break;
				case 2:								//	 	2 - defroster center temperature check
					view.Val_T_2UpdateView(Val);
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
		if (Model::getFlagCurrentVal_H_Chng(sensNum) == 1) {
			int Val = Model::getCurrentVal_H(sensNum);
			Model::clearFlagCurrentVal_H_Chng(sensNum);
			switch (sensNum) {
				case 0:								//	 	0 - defroster left humidity check
					view.Val_H_0UpdateView(Val);
					break;
				case 1:								//	 	1 - defroster right humidity check
					view.Val_H_1UpdateView(Val);
					break;
				case 2:								//	 	2 - defroster center humidity check
					view.Val_H_2UpdateView(Val);
					break;
				case 3:								//	 	3 - fish left humidity check
					view.Val_H_3UpdateView(Val);
					break;
				case 4:								//	 	4 - fish right humidity check
					view.Val_H_4UpdateView(Val);
					break;
				default:
					break;
			}
		}
	}
}
