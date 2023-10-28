#include <gui/containers/TextContainer.hpp>

TextContainer::TextContainer()
{

}

void TextContainer::initialize()
{
    TextContainerBase::initialize();
}

// функция включения элементов в список прокрутки
void TextContainer::updateTextItem(touchgfx::Unicode::UnicodeChar *text)
{
    Unicode::snprintf(textAreaBuffer, TEXTAREA_SIZE, "%s", text);
    textArea.invalidate();
}
