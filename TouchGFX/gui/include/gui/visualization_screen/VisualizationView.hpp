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

    virtual void Val_T_0UpdateView(int val);
    virtual void Val_T_1UpdateView(int val);
    virtual void Val_T_2UpdateView(int val);
    virtual void Val_T_3UpdateView(int val);
    virtual void Val_T_4UpdateView(int val);

    virtual void Val_H_0UpdateView(int val);
    virtual void Val_H_1UpdateView(int val);
    virtual void Val_H_2UpdateView(int val);
    virtual void Val_H_3UpdateView(int val);
    virtual void Val_H_4UpdateView(int val);
protected:

};

#endif // VISUALIZATIONVIEW_HPP
