#include <gui/containers/TextSelectedContainer.hpp>

TextSelectedContainer::TextSelectedContainer()
{

}

void TextSelectedContainer::initialize()
{
    TextSelectedContainerBase::initialize();
}

// функция включения элементов в список прокрутки
void TextSelectedContainer::updateTextSelectedItem(char *text, int num)
{
    Unicode::snprintf(textAreaSelectedBuffer, TEXTAREASELECTED_SIZE, text, num);
    textAreaSelected.invalidate();
}
