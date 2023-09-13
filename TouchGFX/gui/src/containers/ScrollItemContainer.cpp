#include <gui/containers/ScrollItemContainer.hpp>

ScrollItemContainer::ScrollItemContainer()
{

}

void ScrollItemContainer::initialize()
{
    ScrollItemContainerBase::initialize();
}

void ScrollItemContainer::updateScrollItem(int16_t value)
{
    Unicode::snprintf(ScrollItemBuffer, SCROLLITEM_SIZE, "%d", value);
    ScrollItem.invalidate();
}
