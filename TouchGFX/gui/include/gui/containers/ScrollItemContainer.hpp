#ifndef SCROLLITEMCONTAINER_HPP
#define SCROLLITEMCONTAINER_HPP

#include <gui_generated/containers/ScrollItemContainerBase.hpp>

class ScrollItemContainer : public ScrollItemContainerBase
{
public:
    ScrollItemContainer();
    virtual ~ScrollItemContainer() {}

    virtual void initialize();
    void updateScrollItem(int16_t value);
protected:
};

#endif // SCROLLITEMCONTAINER_HPP
