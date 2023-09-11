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

    virtual void Val0UpdateView(int val);
    virtual void Val1UpdateView(int val);
    virtual void Val2UpdateView(int val);
    virtual void Val3UpdateView(int val);
    virtual void Val4UpdateView(int val);
protected:

};

#endif // VISUALIZATIONVIEW_HPP
