#ifndef SCROLLSELECTEDITEMCONTAINER_HPP
#define SCROLLSELECTEDITEMCONTAINER_HPP

#include <gui_generated/containers/ScrollSelectedItemContainerBase.hpp>

class ScrollSelectedItemContainer : public ScrollSelectedItemContainerBase
{
public:
    ScrollSelectedItemContainer();
    virtual ~ScrollSelectedItemContainer() {}

    virtual void initialize();
    virtual void updateScrollSelectedItem(int value);	// функция наполнения контейнера элементом
protected:
};

#endif // SCROLLSELECTEDITEMCONTAINER_HPP
