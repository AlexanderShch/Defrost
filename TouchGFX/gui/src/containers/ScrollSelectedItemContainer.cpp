#include <gui/containers/ScrollSelectedItemContainer.hpp>

ScrollSelectedItemContainer::ScrollSelectedItemContainer()
{

}

void ScrollSelectedItemContainer::initialize()
{
    ScrollSelectedItemContainerBase::initialize();
}

void ScrollSelectedItemContainer::updateScrollSelectedItem(int16_t value)
{
    Unicode::snprintf(ScrollSelectedItemBuffer, SCROLLSELECTEDITEM_SIZE, "%d", value);
    ScrollSelectedItem.invalidate();
}
