#ifndef VISUALIZATIONVIEW_HPP
#define VISUALIZATIONVIEW_HPP

#include <gui_generated/visualization_screen/VisualizationViewBase.hpp>
#include <gui/visualization_screen/VisualizationPresenter.hpp>

class VisualizationView : public VisualizationViewBase
{
public:
    VisualizationView();
    virtual ~VisualizationView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    virtual void Val_T_0UpdateView(short Val);
    virtual void Val_T_1UpdateView(short Val);
    virtual void Val_T_2UpdateView(short Val);
    virtual void Val_T_3UpdateView(short Val);
    virtual void Val_T_4UpdateView(short Val);

    virtual void Val_H_0UpdateView(short Val);
    virtual void Val_H_1UpdateView(short Val);
    virtual void Val_H_2UpdateView(short Val);
    virtual void Val_H_3UpdateView(short Val);
    virtual void Val_H_4UpdateView(short Val);

    virtual void Val_Ten1_Left_UpdateView(uint8_t val);
    virtual void Val_Ten2_Left_UpdateView(uint8_t val);
    virtual void Val_Ten1_Right_UpdateView(uint8_t val);
    virtual void Val_Ten2_Right_UpdateView(uint8_t val);
    virtual void Val_Vent1_Left_UpdateView(uint8_t val);
    virtual void Val_Vent2_Left_UpdateView(uint8_t val);
    virtual void Val_Vent1_Right_UpdateView(uint8_t val);
    virtual void Val_Vent2_Right_UpdateView(uint8_t val);
    virtual void Val_Water_Flap_UpdateView(uint8_t val);

    void AnimHeat12_Switch(void);
    void AnimHeat34_Switch(void);
    void AnimFan12_Switch(void);
    void AnimFan34_Switch(void);

    /** Аварийная подсветка StateHeat/StateFan по Model::Device_AlarmFlags; без аварии — как по DFR_current. */
    void syncDeviceAlarmIndicators(void);

    /** Вытяжка AnimFan_Out по входу Vent_Out; заслонки Flap / Flap_Open / Flap_Close по Air_Open, Air_Close (Model::DI_DFR). */
    void syncExhaustFanAndFlapFromInputs(void);

protected:

};



#endif // VISUALIZATIONVIEW_HPP
