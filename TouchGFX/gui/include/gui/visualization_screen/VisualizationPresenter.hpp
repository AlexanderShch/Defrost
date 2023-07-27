#ifndef VISUALIZATIONPRESENTER_HPP
#define VISUALIZATIONPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

/*	Current temperature from sensor saved in CurrentTemperature[SQ] array
 * 	0 - defroster left
 * 	1 - defroster right
 * 	2 - defroster center
 *	3 - fish left
 *	4 - fish right
 */


using namespace touchgfx;

class VisualizationView;

class VisualizationPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    VisualizationPresenter(VisualizationView& v);

    /**
     * The activate function is called automatically when this screen is "switched in"
     * (ie. made active). Initialization logic can be placed here.
     */
    virtual void activate();

    /**
     * The deactivate function is called automatically when this screen is "switched out"
     * (ie. made inactive). Teardown functionality can be placed here.
     */
    virtual void deactivate();

    virtual ~VisualizationPresenter() {};

    virtual void ValUpdatePresenter();

private:
    VisualizationPresenter();

    VisualizationView& view;
};

#endif // VISUALIZATIONPRESENTER_HPP
