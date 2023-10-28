#ifndef DIGITSELECTEDCONTAINER_HPP
#define DIGITSELECTEDCONTAINER_HPP

#include <gui_generated/containers/DigitSelectedContainerBase.hpp>

class DigitSelectedContainer : public DigitSelectedContainerBase
{
public:
    DigitSelectedContainer();
    virtual ~DigitSelectedContainer() {}

    virtual void initialize();
    virtual void updateDigitSelectedItem(int value);	// функция наполнения контейнера элементом
protected:
};

#endif // DIGITSELECTEDCONTAINER_HPP
