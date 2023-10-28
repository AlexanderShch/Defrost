#include <gui/containers/TextSelectedContainer.hpp>

TextSelectedContainer::TextSelectedContainer()
{

}

void TextSelectedContainer::initialize()
{
    TextSelectedContainerBase::initialize();
}

// функция включения элементов в список прокрутки
void TextSelectedContainer::updateTextSelectedItem(char *text)
{
    Unicode::snprintf(textAreaSelectedBuffer, TEXTAREASELECTED_SIZE, "%s", text);
    textAreaSelected.invalidate();
}
