# ⚡ Быстрый старт: Отображение версии в TouchGFX

## 🎯 Что делать СЕЙЧАС

### 1. Сгенерируйте код в TouchGFX Designer
```
TouchGFX Designer → кнопка "Generate Code" (F4)
```

### 2. Раскомментируйте код
Откройте: `TouchGFX/gui/src/home_screen/HomeView.cpp`

**Найдите метод `updateVersionDisplay()` (строка ~48)**

Удалите `/*` и `*/`:

**БЫЛО:**
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

**СТАЛО:**
```cpp
void HomeView::updateVersionDisplay()
{
    const char* version = Model::getFirmwareVersion();
    Unicode::strncpy(VersionValueBuffer, version, VERSIONVALUE_SIZE);
    LabelVerID.invalidate();
}
```

### 3. Пересоберите проект
```
STM32CubeIDE → Project → Build All (Ctrl+B)
```

### 4. Загрузите и проверьте
Загрузите прошивку → откройте экран Home → версия "1.1.0" должна отображаться! ✅

---

## 📖 Полная документация
См. файл: `docs/TouchGFX_Version_Display_Setup.md`

---

**Версия прошивки**: 1.1.0  
**Дата**: 12 ноября 2025

