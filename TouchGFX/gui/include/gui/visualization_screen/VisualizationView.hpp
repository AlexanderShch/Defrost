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

    virtual void Val0UpdateView(float val);
    virtual void Val1UpdateView(float val);
    virtual void Val2UpdateView(float val);
    virtual void Val3UpdateView(float val);
    virtual void Val4UpdateView(float val);
protected:

};

#endif // VISUALIZATIONVIEW_HPP
