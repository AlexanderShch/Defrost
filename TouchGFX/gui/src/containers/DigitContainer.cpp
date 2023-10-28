#include <gui/containers/DigitContainer.hpp>

DigitContainer::DigitContainer()
{

}

void DigitContainer::initialize()
{
    DigitContainerBase::initialize();
}
// функция включения элементов в список прокрутки
void DigitContainer::updateDigitItem(int value)
{
    Unicode::snprintf(ScrollDigitBuffer, SCROLLDIGIT_SIZE, "%d", value);
    ScrollDigit.invalidate();
}
