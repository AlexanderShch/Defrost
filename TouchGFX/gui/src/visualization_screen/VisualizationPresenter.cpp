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
		if (Model::getFlagCurrentValChng(sensNum) == 1) {
			float Val = Model::getCurrentVal(sensNum);
			switch (sensNum) {
				case 0:								//	 	0 - defroster left temperature check
					view.Val0UpdateView(Val);
					Model::clearFlagCurrentValChng(sensNum);
					break;
				case 1:								//	 	1 - defroster right temperature check
					view.Val1UpdateView(Val);
					Model::clearFlagCurrentValChng(sensNum);
					break;
				case 2:								//	 	2 - defroster center temperature check
					view.Val2UpdateView(Val);
					Model::clearFlagCurrentValChng(sensNum);
					break;
				case 3:								//	 	3 - fish left temperature check
					view.Val3UpdateView(Val);
					Model::clearFlagCurrentValChng(sensNum);
					break;
				case 4:								//	 	4 - fish right temperature check
					view.Val4UpdateView(Val);
					Model::clearFlagCurrentValChng(sensNum);
					break;
				default:
					break;
			}
		}
	}
}

