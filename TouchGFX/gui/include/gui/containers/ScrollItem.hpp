#ifndef SCROLLITEM_HPP
#define SCROLLITEM_HPP

#include <gui_generated/containers/ScrollItemBase.hpp>

class ScrollItem : public ScrollItemBase
{
public:
    ScrollItem();
    virtual ~ScrollItem() {}

    virtual void initialize();
protected:
};

#endif // SCROLLITEM_HPP
