#include <gui/visualization_screen/VisualizationView.hpp>
#include <gui/visualization_screen/VisualizationPresenter.hpp>


//extern volatile DFR_REGISTERS_t DFR;				// Объявление регистра состояния управления устройствами
//extern volatile DFR_REGISTERS_t DFR_current;		// Объявление регистра текущего отображения состояния управления устройствами
//extern volatile DFR_REGISTERS_t DFR_chng_flag;		// Объявление регистра флагов изменения состояния управления устройствами
uint16_t FlagsTemp;
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
	// Закончено обновление Т и Н
	// Обновим работу оборудования
	// Проверим флаги смены состояния и передадим флаг состояния во view
	if (Model::DFR_chng_flag.Ten1_Left == 1)
	{
	    view.Val_Ten1_Left_UpdateView(Model::DFR.Ten1_Left);
	};
    if (Model::DFR_chng_flag.Ten2_Left == 1)
	{
    	view.Val_Ten2_Left_UpdateView(Model::DFR.Ten2_Left);
	};
    if (Model::DFR_chng_flag.Ten1_Right == 1)
	{
    	view.Val_Ten1_Right_UpdateView(Model::DFR.Ten1_Right);
	};
    if (Model::DFR_chng_flag.Ten2_Right == 1)
	{
    	view.Val_Ten2_Right_UpdateView(Model::DFR.Ten2_Right);
	};
    if (Model::DFR_chng_flag.Vent1_Left == 1)
	{
    	view.Val_Vent1_Left_UpdateView(Model::DFR.Vent1_Left);
	};
    if (Model::DFR_chng_flag.Vent2_Left == 1)
	{
    	view.Val_Vent2_Left_UpdateView(Model::DFR.Vent2_Left);
	};
    if (Model::DFR_chng_flag.Vent1_Right == 1)
	{
    	view.Val_Vent1_Right_UpdateView(Model::DFR.Vent1_Right);
	};
    if (Model::DFR_chng_flag.Vent2_Right == 1)
	{
    	view.Val_Vent2_Right_UpdateView(Model::DFR.Vent2_Right);
	};
    if (Model::DFR_chng_flag.Water_Flap == 1)
	{
    	view.Val_Water_Flap_UpdateView(Model::DFR.Water_Flap);
	};

//	DFR_chng_flag.Vent1_Left = 1;
//	DFR_current.Vent2_Left = 1;
//	FlagsTemp = *(uint16_t*) &DFR;
//	FlagsTemp = *(uint16_t*) &DFR_chng_flag;
//			//*(uint16_t*) &DFR ^ *(uint16_t*) &DFR_current;
}

