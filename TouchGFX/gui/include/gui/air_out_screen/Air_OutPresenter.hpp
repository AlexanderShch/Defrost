#ifndef AIR_OUTPRESENTER_HPP
#define AIR_OUTPRESENTER_HPP

#include <gui/model/ModelListener.hpp>
#include <mvp/Presenter.hpp>

using namespace touchgfx;

class Air_OutView;

class Air_OutPresenter : public touchgfx::Presenter, public ModelListener
{
public:
    Air_OutPresenter(Air_OutView& v);

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

    virtual ~Air_OutPresenter() {}

private:
    Air_OutPresenter();

    Air_OutView& view;
};

#endif // AIR_OUTPRESENTER_HPP
