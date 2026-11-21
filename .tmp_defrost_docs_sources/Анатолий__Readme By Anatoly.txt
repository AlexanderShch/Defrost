SOURCE: D:\Users\Alsh1004\OneDrive\Projects\Documentation\Проект дефростер\Анатолий\Readme By Anatoly.docx

[P1 | Heading 1] TouchGFX
[P2 | Heading 2] Описание элементов:
[P3 | Normal] Виджеты:
[P4 | Normal] Все наименования виджетов начинаются с типа виджета для лучшей идентификации:
[P5 | List Paragraph] BTN – кнопка
[P6 | List Paragraph] BOX – рамка с подложкой
[P7 | List Paragraph] Value – текстовое поле, в которое передается значение, содержит Wildcard
[P8 | List Paragraph] Label – текстовое поле
[P9 | List Paragraph] State – текстовое поле для отображения состояния (будет удалено в будущих версиях)
[P10 | List Paragraph] IMG – изображение
[P11 | List Paragraph] ANIM – анимация
[P12 | Normal] Interactions:
[P13 | Normal] GoTo… - переход на экран
[P14 | Heading 2] Список экранов
[P15 | Heading 3] Home
[P16 | Normal] Данные для отображения:
[P17 | List Paragraph] Температура ядра, Text ID: ValueCoreT1, ValueCoreT2
[P18 | List Paragraph] Оставшееся время программы: ValuePRGTimeLeft
[P19 | List Paragraph] Кнопка «Старт»: BTNStart, объект FlexButton
[P20 | List Paragraph] Кнопка «Стоп»: BTNStop, объект FlexButton
[P21 | Normal] Interactions:
[P22 | List Paragraph] GoToVisualization – переход на страницу Visualization
[P23 | List Paragraph] GoToSettings1
[P24 | Normal] Планы на доработку:
[P25 | List Paragraph] Смена состояния кнопок Старт и Стоп для исключения состояния, когда нажаты обе кнопки.
[P26 | Heading 3] Visualization
[P27 | Normal] Данные для отображения:
[P28 | List Paragraph] Состояние тэнов 1-4. На данный момент реализовано отображением текстовых полей StateHeat1 … StateHeat4 и запуском анимации AnimHeat12 и AnimHeat34 при включении одно из пары тэнов.
[P29 | List Paragraph] Состояние вентиляторов 1-4. На данный момент реализовано отображением текстовых полей StateFan1 … StateFan4 и запуском анимации AnimFan12 и AnimFan34 при включении одно из пары вентиляторов.
[P30 | List Paragraph] Данные по влажности в камере: ValueDefrosterH
[P31 | List Paragraph] Данные по температуре в камере от двух датчиков: ValueDefrosterT1, ValueDefrosterT2
[P32 | List Paragraph] Данные по температуре в рыбе от двух датчиков: ValueCoreT1Small, ValueCoreT2Small (Text ID с припиской Small, т.к. Typography используется Small, 10px)
[P33 | Normal] Interactions:
[P34 | List Paragraph] GoToHome
[P35 | List Paragraph] GoToSettingsMenu
[P36 | Normal] Планы на доработку:
[P37 | List Paragraph] Отказ от отображения текстовых полей, вместо них сделать анимации с двумя парами вентиляторов и двумя парами тэнов. Вентиляторы начинают вращаться, а тэны меняют цвет при включении.
[P38 | Heading 3] SettingsMenu
[P39 | Normal] Данные для отображения:
[P40 | List Paragraph] Нет
[P41 | Normal] Interactions:
[P42 | List Paragraph] GoToHome
[P43 | List Paragraph] GoToSettings1_1 (стрелка в углу экрана)
[P44 | List Paragraph] GoToSettings1
[P45 | List Paragraph] GoToSettings2
[P46 | List Paragraph] GoToSettings3
[P47 | List Paragraph] GoToSettings4
[P48 | Normal] Планы на доработку:
[P49 | List Paragraph] Не планируется
[P50 | Heading 3] Settings1
[P51 | Normal] Данные для отображения:
[P52 | List Paragraph] Использование датчиков температуры в теле продукта. Для этого используется Toggle Button BTNCoreTSensorsState. По умолчанию стоит значение ON.
[P53 | List Paragraph] Температура в теле продукта. Задается через int CoreTSet. Описание ниже в списке переменных
[P54 | Normal] Interactions:
[P55 | List Paragraph] GoToHome
[P56 | List Paragraph] GoToSettings2
[P57 | List Paragraph] GoToSettings4
[P58 | List Paragraph] GoToSettingsMenu
[P59 | List Paragraph] BTNCoreTSetIncreaseClicked – вызов виртуальной функции BTNCoreTSetIncreaseClicked, отвечающей за увеличение значения CoreTSet и ограничения её в 8 градусов.
[P60 | List Paragraph] BTNCoreTSetDecreaseClicked – аналогично Increse, устанавливает ограничение температуры не ниже 0 градусов.
[P61 | Normal] Планы на доработку:
[P62 | Normal] Возможно отключение датчика по одному и отображение их значений. Надо думать.
[P63 | Heading 3] Settings2
[P64 | Normal] Данные для отображения:
[P65 | Normal] Состояние ворот указывается кнопкой BTNGateControl. OFF = закрыто.
[P66 | Normal] Клапан воды для форсунок указывается кнопкой BTNSprayControl. OFF = закрыто
[P67 | Normal] Interactions:
[P68 | List Paragraph] GoToHome
[P69 | List Paragraph] GoToSettings1
[P70 | List Paragraph] GoToSettings3
[P71 | List Paragraph] GoToSettingsMenu
[P72 | List Paragraph] BTNGateManualClicked - вызов виртуальной функции BTNGateManualClicked, отображающей кнопку для открытия и закрытия ворот.
[P73 | List Paragraph] BTNSprayManualClicked - вызов виртуальной функции BTNSprayManualClicked, отображающей кнопку для открытия и закрытия ворот и отключающий форсунки.
[P74 | Normal] Планы на доработку:
[P75 | List Paragraph] Нет
[P76 | Heading 3] Settings3
[P77 | Normal] Данные для отображения:
[P78 | List Paragraph] Состояние вентиляторов 1, 2: BTNFanControl1, BTNFanControl2
[P79 | List Paragraph] Состояние тэнов 1, 2: BTNHeatControl1, BTNHeatControl2
[P80 | Normal] Interactions:
[P81 | List Paragraph] GoToHome
[P82 | List Paragraph] GoToSettings1
[P83 | List Paragraph] GoToSettings3
[P84 | List Paragraph] GoToSettingsMenu
[P85 | List Paragraph] BTNManual1Clicked – вызов виртуальной функции BTNManual1Clicked. Включение отображения кнопки управления вентиляторами BTNFanControl1. При отключении ручного управления выключается тэн и вентилятор группы 1. Скрываются кнопки управления группы 1.
[P86 | List Paragraph] BTNFanControl1Clicked - вызов виртуальной функции BTNFanControl1Clicked. Включение отображения кнопки управления тэном BTNHeatControl1. При отключении вентилятора выключается тэн. Скрывается кнопка управления тэном.
[P87 | Normal] Планы на доработку:
[P88 | List Paragraph] Нет
[P89 | Heading 3] Settings4
[P90 | Normal] Аналогично Settings3.
[P91 | Heading 2] Список переменных
[P92 | List Paragraph] int CoreTSet – задание температуры ядра. Отображается и изменяется на экране Settings1
[P93 | List Paragraph] Инициализируется в файле: Project_Defrost-Anatoly\TouchGFX\gui\include\gui\settings1_screen\Settings1View.hpp
[P95 | List Paragraph] Изменение значения на экране происходит функциями BTNCoreTSetIncrease, BTNCoreTSetDecrease. Также функциями происходит ограничения изменения значения от 0 до 8 градусов цельсия
[P96 | List Paragraph] Функции описаны в файле:
[P97 | List Paragraph] Project_Defrost-Anatoly\TouchGFX\gui\src\settings1_screen\Settings1View.cpp
[P98 | List Paragraph] int DefrosterT1 – температура в дефростере, датчик №1.
[P99 | Normal] Settings5
[P100 | Normal] Данные для отображения:
[P101 | Normal] Экран программирования датчиков. Выбирается тип датчика через всплывающее окно. Отображается текущие скорость и адрес датчика. Через всплывающие окна выбираются новые скорость и адрес датчика. Присутствует кнопка записи для отправки новых значений в датчик.
[P102 | Normal] Interactions:
[P103 | List Paragraph] GoToHome
[P104 | List Paragraph] GoToSettings1
[P105 | List Paragraph] GoToSettings3
[P106 | List Paragraph] GoToSettingsMenu
[P107 | List Paragraph] BTNSensorTypeClicked – вызов виртуальной функции, отображение скроллового окна на экране. Подробнее смотри ниже.
[P108 | Normal] Планы на доработку:
[P109 | Heading 2] Работа скроллового окна
[P110 | List Paragraph] Инструкция: https://support.touchgfx.com/docs/development/ui-development/ui-components/containers/scroll-wheel#user-code
[P111 | List Paragraph] В TouchGFX необходимо создать контейнеры с элементами списка и если требуется, то контейнер, который будет описывать активный элемент.
[P112 | List Paragraph] При создании ScrollList на экране в TouchGFX указывается количество элементов и «элемент по умолчанию» и контейнеры для отображения данных.
[P113 | List Paragraph] .cpp файлы контейнеров находятся по адресу: Defrost\TouchGFX\gui\src\containers
[P114 | List Paragraph] .hpp файлы контейнеров находятся по адресу: Defrost\TouchGFX\gui\include\gui\containers
[P115 | List Paragraph] В .cpp прописывается код для обновления содержимого текстового поля ScrollItem в самом контейнере:
[P116 | List Paragraph] void ScrollItemContainer::updateScrollItem(int16_t value)
[P117 | List Paragraph] {
[P118 | List Paragraph] Unicode::snprintf(ScrollItemBuffer, SCROLLITEM_SIZE, "%d", value);
[P119 | List Paragraph] ScrollItem.invalidate();
[P120 | List Paragraph] }
[P122 | List Paragraph] В .hpp прописывается функция: void updateScrollItem(int16_t value);
[P123 | List Paragraph] Аналогично для ScrollSelectedItemContainer:
[P124 | Normal (Web)] .cpp:
[P125 | Normal (Web)] void ScrollSelectedItemContainer::updateScrollSelectedItem(int16_t value)
[P126 | Normal (Web)] {
[P127 | Normal (Web)] Unicode::snprintf(ScrollSelectedItemBuffer, SCROLLSELECTEDITEM_SIZE, "%d", value);
[P128 | Normal (Web)] ScrollSelectedItem.invalidate();
[P129 | Normal (Web)] }
[P130 | Normal] .hpp:
[P131 | Normal] void updateScrollSelectedItem(int16_t value);
[P132 | List Paragraph] В файлах экранов уже идет обращение к конкретному «скроллу». Может быть несколько скроллов, которые будут использовать одни и теже контейнеры для вывода содержимого (хоть текст, хоть картинку, смену цвета и вообще что хочешь).
[P133 | List Paragraph] Чтобы начать изменять данные скролла нужно взять код его функции в файле: \Defrost\TouchGFX\generated\gui_generated\include\gui_generated\settings5_screen\ Settings5ViewBase.hpp
[P134 | List Paragraph] В файле .cpp экрана будет следующее (на примере selected item, где кроме обновления содержимого «центрального» контейнера еще записывается переменная int SetSpeed, которая при нажатии кнопки «записать» отобразится на кнопке выбора скорости и может быть записана в датчик. Почему-то прошёл сдвиг в updateScrollSelectedItem и записи переменной на -1):
[P135 | Normal (Web)] void Settings5View::scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex)
[P136 | Normal (Web)] {
[P137 | Normal (Web)] switch (itemIndex)
[P138 | Normal (Web)] {
[P139 | Normal (Web)] case 0:
[P140 | Normal (Web)] item.updateScrollSelectedItem(2400);
[P141 | Normal (Web)] SetSpeed = 115200;
[P142 | Normal (Web)] break;
[P143 | Normal (Web)] case 1:
[P144 | Normal (Web)] item.updateScrollSelectedItem(4800);
[P145 | Normal (Web)] SetSpeed = 2400;
[P146 | Normal (Web)] break;
[P147 | Normal (Web)] case 2:
[P148 | Normal (Web)] item.updateScrollSelectedItem(9600);
[P149 | Normal (Web)] SetSpeed = 4800;
[P150 | Normal (Web)] break;
[P151 | Normal (Web)] case 3:
[P152 | Normal (Web)] item.updateScrollSelectedItem(19200);
[P153 | Normal (Web)] SetSpeed = 9600;
[P154 | Normal (Web)] break;
[P155 | Normal (Web)] case 4:
[P156 | Normal (Web)] item.updateScrollSelectedItem(38400);
[P157 | Normal (Web)] SetSpeed = 19200;
[P158 | Normal (Web)] break;
[P159 | Normal (Web)] case 5:
[P160 | Normal (Web)] item.updateScrollSelectedItem(115200);
[P161 | Normal (Web)] SetSpeed = 38400;
[P162 | Normal (Web)] break;
[P163 | Normal (Web)] }
[P164 | Normal (Web)] }
[P165 | Normal (Web)] В файле .hpp экрана:
[P166 | Normal (Web)] virtual void scrollSensorSpeedNewUpdateItem(ScrollItemContainer& item, int16_t itemIndex);
[P167 | Normal (Web)] virtual void scrollSensorSpeedNewUpdateCenterItem(ScrollSelectedItemContainer& item, int16_t itemIndex);
[P171 | Heading 2] Вызов функции С++ из С
[P172 | Normal] Если Вы хотите вызвать функцию - члена класса (включая функции virtual) из C, то понадобится реализовать простую обертку. Например:
[P173 | Normal] // Код на языке C++:
[P174 | Normal] class C
[P175 | Normal] {
[P176 | Normal] // ...
[P177 | Normal] virtual double f(int);
[P178 | Normal] };
[P180 | Normal] // Функция-обертка над функцией f(int):
[P181 | Normal] extern "C" double call_C_f(C* p, int i)
[P182 | Normal] {
[P183 | Normal] return p->f(i);
[P184 | Normal] }
[P185 | Normal] Теперь функция C::f() может быть вызвана следующим образом:
[P186 | Normal] // Код на языке C:
[P187 | Normal] double call_C_f(struct C* p, int i);
[P189 | Normal] void ccc(struct C* p, int i)
[P190 | Normal] {
[P191 | Normal] double d = call_C_f(p,i);
[P192 | Normal] // ...
[P193 | Normal] }
[P195 | Heading 1] MySQL
[P196 | Heading 2] Базовые настройки сервера и базы данных
[P197 | Normal] TCP/IP соединение, Port 3306, X Protocol Port 33060
[P198 | Normal] Root Password: 1234
[P199 | Normal] Users (Name / Password / Role):
[P200 | Normal] Admin / 1234 / DB Admin
[P201 | Normal] +
[P202 | Normal] pip install mysql-connector-python
