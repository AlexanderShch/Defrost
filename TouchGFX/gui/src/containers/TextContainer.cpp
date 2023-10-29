#include <gui/containers/TextContainer.hpp>

TextContainer::TextContainer()
{

}

void TextContainer::initialize()
{
    TextContainerBase::initialize();
}

// функция включения элементов в список прокрутки
void TextContainer::updateTextItem(char *text, int num)
{
    Unicode::snprintf(textAreaBuffer, TEXTAREA_SIZE, text, num);
    textArea.invalidate();
}
