# 📱 Настройка отображения версии прошивки в TouchGFX

**Дата**: 12 ноября 2025  
**Компонент**: Home экран  
**Wildcard**: VersionValue  
**Версия прошивки**: 1.1.0

---

## ✅ Что уже сделано

### 1. Добавлен метод получения версии в Model

**Файл**: `TouchGFX/gui/include/gui/model/Model.hpp`
```cpp
// Получение версии прошивки
static const char* getFirmwareVersion();
```

**Файл**: `TouchGFX/gui/src/model/Model.cpp`
```cpp
#include "version.h"

const char* Model::getFirmwareVersion()
{
    return FW_VERSION_STRING;  // Возвращает "1.1.0"
}
```

### 2. Добавлен метод обновления версии в HomeView

**Файл**: `TouchGFX/gui/include/gui/home_screen/HomeView.hpp`
```cpp
// Установка версии прошивки на экране
void updateVersionDisplay();
```

**Файл**: `TouchGFX/gui/src/home_screen/HomeView.cpp`
- Метод `updateVersionDisplay()` готов к использованию
- Вызов добавлен в `setupScreen()` для автоматического обновления при открытии экрана

---

## 📋 Что нужно сделать ВАМ

### Шаг 1: Убедитесь что в TouchGFX Designer создан элемент

В TouchGFX Designer на экране **Home** должен быть:
- **TextArea** с именем `LabelVerID`
- С включенным **Wildcard 1** = `VersionValue`
- С размером буфера (например, 10 символов достаточно для "1.1.0")

### Шаг 2: Сгенерируйте код в TouchGFX Designer

1. Откройте проект в **TouchGFX Designer**
2. Нажмите кнопку **"Generate Code"** (или F4)
3. Дождитесь завершения генерации

После генерации в файле `TouchGFX/generated/gui_generated/include/gui_generated/home_screen/HomeViewBase.hpp` появятся:

```cpp
class HomeViewBase : public touchgfx::View<HomePresenter>
{
protected:
    // ...
    touchgfx::TextAreaWithOneWildcard LabelVerID;  // ← Ваш label
    
    // Wildcard Buffers
    static const uint16_t VERSIONVALUE_SIZE = 10;  // ← Размер буфера
    touchgfx::Unicode::UnicodeChar VersionValueBuffer[VERSIONVALUE_SIZE];  // ← Буфер
};
```

### Шаг 3: Раскомментируйте код в HomeView.cpp

Откройте файл: `TouchGFX/gui/src/home_screen/HomeView.cpp`

Найдите метод `updateVersionDisplay()` и **раскомментируйте** код:

```cpp
void HomeView::updateVersionDisplay()
{
    // Получаем версию прошивки из Model
    const char* version = Model::getFirmwareVersion();
    
    // Преобразуем строку версии в Unicode и записываем в буфер wildcard
    Unicode::strncpy(VersionValueBuffer, version, VERSIONVALUE_SIZE);
    
    // Обновляем отображение на экране
    LabelVerID.invalidate();
}
```

**Было:**
```cpp
void HomeView::updateVersionDisplay()
{
    // РАСКОММЕНТИРУЙТЕ ЭТОТ КОД ПОСЛЕ ГЕНЕРАЦИИ TouchGFX:
    /*
    const char* version = Model::getFirmwareVersion();
    Unicode::strncpy(VersionValueBuffer, version, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
    */
}
```

**Станет:**
```cpp
void HomeView::updateVersionDisplay()
{
    const char* version = Model::getFirmwareVersion();
    Unicode::strncpy(VersionValueBuffer, version, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
}
```

### Шаг 4: Пересоберите проект

1. В **STM32CubeIDE**: `Project → Build All` (или Ctrl+B)
2. Загрузите прошивку в контроллер
3. Откройте экран **Home**
4. Версия **"1.1.0"** должна отображаться в `LabelVerID`! 🎉

---

## 🔍 Как это работает

### Последовательность вызовов

```
1. Пользователь открывает экран Home
   ↓
2. TouchGFX вызывает HomeView::setupScreen()
   ↓
3. setupScreen() вызывает updateVersionDisplay()
   ↓
4. updateVersionDisplay() вызывает Model::getFirmwareVersion()
   ↓
5. Model возвращает FW_VERSION_STRING из version.h ("1.1.0")
   ↓
6. Строка копируется в VersionValueBuffer
   ↓
7. LabelVerID обновляется и показывает версию на экране
```

### Диаграмма компонентов

```
┌─────────────────────────────────────────────────────────────┐
│                         Home Screen                          │
│                                                               │
│  ┌────────────────────┐                                      │
│  │   LabelVerID       │  ← Отображает версию "1.1.0"        │
│  │ (TextAreaWithWC)   │                                      │
│  └────────┬───────────┘                                      │
│           │                                                   │
│           │ VersionValueBuffer[]                             │
│           │                                                   │
│  ┌────────▼──────────────────────────────────────────┐      │
│  │          HomeView::updateVersionDisplay()         │      │
│  │  1. Получает версию из Model                      │      │
│  │  2. Копирует в буфер wildcard                     │      │
│  │  3. Обновляет отображение                         │      │
│  └────────┬──────────────────────────────────────────┘      │
└───────────┼──────────────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────────────────────────┐
│                          Model                               │
│                                                               │
│  ┌──────────────────────────────────────────┐               │
│  │  Model::getFirmwareVersion()              │               │
│  │  return FW_VERSION_STRING;                │               │
│  └────────┬──────────────────────────────────┘               │
└───────────┼──────────────────────────────────────────────────┘
            │
            ▼
┌─────────────────────────────────────────────────────────────┐
│                     version.h                                │
│                                                               │
│  #define FW_VERSION_STRING "1.1.0"                          │
└─────────────────────────────────────────────────────────────┘
```

---

## 🎨 Настройка отображения в Designer

### Рекомендуемые параметры для LabelVerID

| Параметр | Значение | Описание |
|----------|----------|----------|
| **Type** | TextArea | Текстовая область |
| **Name** | LabelVerID | Имя элемента |
| **Wildcard 1** | VersionValue | Имя wildcard |
| **Buffer Size** | 10-15 | Размер для "1.1.0" + запас |
| **Text** | "ver: <VersionValue>" | Шаблон отображения |
| **Typography** | Small / Default | Размер шрифта |
| **Alignment** | Left / Center | Выравнивание |
| **Color** | #E8F6FB (светло-голубой) | Цвет текста |

### Пример текста в Designer

Вы можете использовать такие варианты текста:

- `"v<VersionValue>"` → отобразится как "v1.1.0"
- `"ver: <VersionValue>"` → отобразится как "ver: 1.1.0"
- `"FW <VersionValue>"` → отобразится как "FW 1.1.0"
- `"<VersionValue>"` → отобразится как "1.1.0"

---

## 🔧 Расширенные возможности

### Вариант 1: Показывать полную информацию о сборке

Если хотите показывать "v1.1.0 (Nov 12 2025)", измените метод:

```cpp
void HomeView::updateVersionDisplay()
{
    // Используем FW_VERSION_FULL вместо FW_VERSION_STRING
    const char* version = FW_VERSION_FULL;  // "Defrost Controller v1.1.0 (Nov 12 2025 14:30:00)"
    
    Unicode::strncpy(VersionValueBuffer, version, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
}
```

⚠️ **Внимание**: Для полной версии нужен больший буфер (50-60 символов)!

### Вариант 2: Только дата сборки

```cpp
void HomeView::updateVersionDisplay()
{
    const char* buildDate = FW_BUILD_DATE;  // "Nov 12 2025"
    Unicode::strncpy(VersionValueBuffer, buildDate, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
}
```

### Вариант 3: Комбинированный вариант

```cpp
void HomeView::updateVersionDisplay()
{
    char versionStr[20];
    snprintf(versionStr, sizeof(versionStr), "v%d.%d.%d", 
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    
    Unicode::strncpy(VersionValueBuffer, versionStr, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
}
```

---

## ❓ Устранение проблем

### Проблема: Версия не отображается

**Причина 1**: Забыли раскомментировать код в `updateVersionDisplay()`
- **Решение**: Удалите `/*` и `*/` вокруг кода

**Причина 2**: Не сгенерировали код в TouchGFX Designer
- **Решение**: Нажмите "Generate Code" в Designer

**Причина 3**: Неправильное имя wildcard
- **Решение**: Проверьте что wildcard называется именно `VersionValue`

### Проблема: Ошибка компиляции "VERSIONVALUE_SIZE not declared"

**Причина**: Код раскомментирован до генерации TouchGFX
- **Решение**: 
  1. Закомментируйте код обратно
  2. Сгенерируйте код в Designer
  3. Раскомментируйте снова

### Проблема: Показывается мусор вместо версии

**Причина**: Буфер слишком маленький
- **Решение**: Увеличьте Buffer Size в Designer до 15-20 символов

---

## 📝 Контрольный список

- [ ] ✅ Создан `LabelVerID` в TouchGFX Designer
- [ ] ✅ Добавлен wildcard `VersionValue`
- [ ] ✅ Нажата кнопка "Generate Code"
- [ ] ✅ Код раскомментирован в `HomeView.cpp`
- [ ] ✅ Проект пересобран
- [ ] ✅ Прошивка загружена в контроллер
- [ ] ✅ Версия отображается на экране Home

---

## 🎉 Результат

После выполнения всех шагов:

- При открытии экрана **Home** автоматически отображается версия прошивки
- Версия берётся из `version.h` и всегда актуальна
- При обновлении версии в `version.h` экран автоматически покажет новую версию
- Не требуется ручное обновление текста в TouchGFX Designer

**Текущая версия**: **1.1.0** ✨

---

**Автор**: AI Assistant (Claude)  
**Дата**: 12 ноября 2025  
**Версия документа**: 1.0

