#include <gui/containers/ScrollItemContainer.hpp>

ScrollItemContainer::ScrollItemContainer()
{

}

void ScrollItemContainer::initialize()
{
    ScrollItemContainerBase::initialize();
}
// функция включения элементов в список прокрутки
void ScrollItemContainer::updateScrollItem(int value)
{
    Unicode::snprintf(ScrollItemBuffer, SCROLLITEM_SIZE, "%d", value);
    ScrollItem.invalidate();
}
