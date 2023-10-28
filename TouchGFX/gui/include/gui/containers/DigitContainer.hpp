#ifndef DIGITCONTAINER_HPP
#define DIGITCONTAINER_HPP

#include <gui_generated/containers/DigitContainerBase.hpp>

class DigitContainer : public DigitContainerBase
{
public:
    DigitContainer();
    virtual ~DigitContainer() {}

    virtual void initialize();
    virtual void updateDigitItem(int value);	// функция включения элементов в список прокрутки
protected:
};

#endif // DIGITCONTAINER_HPP
