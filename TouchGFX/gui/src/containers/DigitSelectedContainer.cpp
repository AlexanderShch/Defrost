#include <gui/containers/DigitSelectedContainer.hpp>

DigitSelectedContainer::DigitSelectedContainer()
{

}

void DigitSelectedContainer::initialize()
{
    DigitSelectedContainerBase::initialize();
}
// функция наполнения контейнера элементом
void DigitSelectedContainer::updateDigitSelectedItem(int value)
{
    Unicode::snprintf(ScrollSelectedDigitBuffer, SCROLLSELECTEDDIGIT_SIZE, "%d", value);
    ScrollSelectedDigit.invalidate();
}
