#ifndef VISUALIZATIONVIEW_HPP
#define VISUALIZATIONVIEW_HPP

#include <gui_generated/visualization_screen/VisualizationViewBase.hpp>
#include <gui/visualization_screen/VisualizationPresenter.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include <touchgfx/widgets/canvas/PainterRGB565.hpp>

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

    /** Вытяжка AnimFan_Out по Vent_Out; заслонки — по Water_Flap + Air_Open (Model::DI_DFR). */
    void syncExhaustFanAndFlapFromInputs(void);

    /** Анимации по Model::DFR_current (работает/выключено); каждый тик, не зависит от DEVICE_SWITCH_CHECK_ENABLED. */
    void syncEquipmentAnimationsFromDisplayedState(void);

    /** Две капли над рыбками: видны только при включённой форсунке (_Inj). */
    void syncInjectorDropIndicator(void);

protected:
    touchgfx::Circle injDropLeft;
    touchgfx::PainterRGB565 injDropLeftPainter;
    touchgfx::Circle injDropRight;
    touchgfx::PainterRGB565 injDropRightPainter;

};



#endif // VISUALIZATIONVIEW_HPP
