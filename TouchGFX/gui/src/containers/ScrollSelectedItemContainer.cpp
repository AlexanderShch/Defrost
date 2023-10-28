#include <gui/containers/ScrollSelectedItemContainer.hpp>

ScrollSelectedItemContainer::ScrollSelectedItemContainer()
{

}

void ScrollSelectedItemContainer::initialize()
{
    ScrollSelectedItemContainerBase::initialize();
}
// функция наполнения контейнера элементом
void ScrollSelectedItemContainer::updateScrollSelectedItem(int value)
{
    Unicode::snprintf(ScrollSelectedItemBuffer, SCROLLSELECTEDITEM_SIZE, "%d", value);
    ScrollSelectedItem.invalidate();
}
