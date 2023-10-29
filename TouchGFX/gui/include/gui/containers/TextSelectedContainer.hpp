#ifndef TEXTSELECTEDCONTAINER_HPP
#define TEXTSELECTEDCONTAINER_HPP

#include <gui_generated/containers/TextSelectedContainerBase.hpp>

class TextSelectedContainer : public TextSelectedContainerBase
{
public:
    TextSelectedContainer();
    virtual ~TextSelectedContainer() {}

    virtual void initialize();
    virtual void updateTextSelectedItem(char *text, int num);	// функция включения элементов в список прокрутки
protected:
};

#endif // TEXTSELECTEDCONTAINER_HPP
