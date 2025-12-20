#include <gui/home_screen/HomeView.hpp>
#include <gui/model/Model.hpp>
#include <touchgfx/Unicode.hpp>

HomeView::HomeView()
{

}

void HomeView::setupScreen()
{
    HomeViewBase::setupScreen();
    
    // Устанавливаем версию прошивки при инициализации экрана
    updateVersionDisplay();
}

void HomeView::tearDownScreen()
{
    HomeViewBase::tearDownScreen();
}

// 3 - fish left temperature
void HomeView::Val_T_3UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT1Buffer, sizeof(ValueCoreT1Buffer), "%.1f", (float)Val/10);
	ValueCoreT1.invalidate();
}

// 4 - fish right temperature
void HomeView::Val_T_4UpdateView(int Val)
{
	Unicode::snprintfFloat(ValueCoreT2Buffer, sizeof(ValueCoreT2Buffer), "%.1f", (float)Val/10);
	ValueCoreT2.invalidate();
}

/*
 * Функция: updateVersionDisplay
 * Описание: Устанавливает текущую версию прошивки в wildcard VersionValue
 * 
 * Почему: используем сгенерированные LabelVersion/LabelVersionBuffer из TouchGFX Designer.
 */
void HomeView::updateVersionDisplay()
{
    // Получаем версию прошивки из Model
    const char* version = Model::getFirmwareVersion();
    
    // Преобразуем строку версии в Unicode и записываем в буфер wildcard.
    Unicode::strncpy(LabelVersionBuffer, version, LABELVERSION_SIZE);
    LabelVersionBuffer[LABELVERSION_SIZE - 1] = 0;

    // Обновляем отображение на экране.
    LabelVersion.invalidate();
}
