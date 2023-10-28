#ifndef SCROLLITEMCONTAINER_HPP
#define SCROLLITEMCONTAINER_HPP

#include <gui_generated/containers/ScrollItemContainerBase.hpp>

class ScrollItemContainer : public ScrollItemContainerBase
{
public:
    ScrollItemContainer();
    virtual ~ScrollItemContainer() {}

    virtual void initialize();
    virtual void updateScrollItem(int value);	// функция включения элементов в список прокрутки
protected:
};

#endif // SCROLLITEMCONTAINER_HPP
