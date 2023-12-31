#ifndef TEXTCONTAINER_HPP
#define TEXTCONTAINER_HPP

#include <gui_generated/containers/TextContainerBase.hpp>

class TextContainer : public TextContainerBase
{
public:
    TextContainer();
    virtual ~TextContainer() {}

    virtual void initialize();
    virtual void updateTextItem(char *text, int num);	// функция включения элементов в список прокрутки

protected:
};

#endif // TEXTCONTAINER_HPP
