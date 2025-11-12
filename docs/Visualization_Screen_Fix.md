# 🔧 Исправление отображения значений на экране Visualization

**Проблема**: При открытии экрана Visualization не сразу отображаются значения температуры (T) и влажности (H)  
**Дата исправления**: 12 ноября 2025  
**Версия прошивки**: 1.1.0

---

## 🔴 Описание проблемы

### Симптомы

При переходе на экран **Visualization**:
- ❌ Значения температуры (T) не отображаются сразу
- ❌ Значения влажности (H) не отображаются сразу
- ✅ Состояние оборудования (вентиляторы, ТЭНы) отображается корректно
- ⏱️ Значения появляются только после обновления данных (через ~1 секунду)

### Причина

В методе `VisualizationPresenter::activate()` устанавливались флаги изменения **только для оборудования**, но **не для датчиков T/H**.

**Старый код (строка 15-21):**
```cpp
void VisualizationPresenter::activate()
{
    // Инициализация отображения работы устройств
    uint16_t *pDFR_chng_flag = (uint16_t*) &Model::DFR_chng_flag;
    *pDFR_chng_flag = 0b1111111111111111;  // ← Только оборудование!
    VisualizationPresenter::ValUpdatePresenter();
}
```

**Проблема:**
- Метод `ValUpdatePresenter()` проверяет флаги `getFlagCurrentVal_T_Chng()` и `getFlagCurrentVal_H_Chng()`
- Эти флаги **не устанавливались** при открытии экрана
- Поэтому значения T/H **не отображались** до следующего обновления

---

## ✅ Решение

### Новый код

**Файл**: `TouchGFX/gui/src/visualization_screen/VisualizationPresenter.cpp`

```cpp
void VisualizationPresenter::activate()
{
    // ═══════════════════════════════════════════════════════════════════════════
    // ИНИЦИАЛИЗАЦИЯ ОТОБРАЖЕНИЯ ПРИ ОТКРЫТИИ ЭКРАНА
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Устанавливаем флаги изменения для ВСЕХ датчиков температуры и влажности
    // Это заставит отобразить текущие значения сразу при открытии экрана
    for (int sensNum = 0; sensNum < SQ; ++sensNum)
    {
        // Получаем текущие значения из Model
        short currentT = Model::getCurrentVal_T(sensNum);
        short currentH = Model::getCurrentVal_H(sensNum);
        
        // Устанавливаем значения с установкой флагов
        Model::setCurrentVal_T(sensNum, currentT);
        Model::setCurrentVal_H(sensNum, currentH);
    }
    
    // Инициализация отображения работы устройств (оборудование)
    uint16_t *pDFR_chng_flag = (uint16_t*) &Model::DFR_chng_flag;
    *pDFR_chng_flag = 0b1111111111111111;
    
    // Обновляем отображение всех значений
    VisualizationPresenter::ValUpdatePresenter();
}
```

### Что делает код

1. **Цикл по всем датчикам** (0-6):
   - Получает текущие значения T и H из Model
   - Вызывает `setCurrentVal_T()` и `setCurrentVal_H()`
   - Эти методы **автоматически устанавливают флаги** изменения

2. **Инициализация оборудования** (как раньше):
   - Устанавливает флаги для ТЭНов и вентиляторов

3. **Обновление отображения**:
   - Вызывает `ValUpdatePresenter()`
   - Который теперь видит установленные флаги и обновляет экран

---

## 🔍 Как это работает

### Последовательность при открытии экрана Visualization

```
1. Пользователь переходит на экран Visualization
   ↓
2. TouchGFX вызывает VisualizationPresenter::activate()
   ↓
3. Цикл for проходит по всем датчикам (0-6):
   ├─> Получает текущее значение T из Model
   ├─> Вызывает setCurrentVal_T() ← Устанавливает флаг!
   ├─> Получает текущее значение H из Model
   └─> Вызывает setCurrentVal_H() ← Устанавливает флаг!
   ↓
4. Устанавливаются флаги оборудования (DFR_chng_flag)
   ↓
5. Вызывается ValUpdatePresenter()
   ├─> Проверяет флаги T/H для каждого датчика
   ├─> Находит установленные флаги ✅
   └─> Вызывает Val_T_X_UpdateView() и Val_H_X_UpdateView()
   ↓
6. Экран отображает ВСЕ текущие значения сразу! ✨
```

### Диаграмма данных

```
┌─────────────────────────────────────────────────────────────────┐
│                          Model                                   │
│                                                                   │
│  CurrentValueT[7] = {250, 230, 240, -50, -45, 180, 0}           │
│  CurrentValueH[7] = {650, 680, 700, 0, 0, 0, 0}                 │
│                                                                   │
│  FlagCurrentValueTChanged[7] = {0, 0, 0, 0, 0, 0, 0} ← Нули!   │
│  FlagCurrentValueHChanged[7] = {0, 0, 0, 0, 0, 0, 0} ← Нули!   │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             │ activate() вызывается
                             ↓
┌─────────────────────────────────────────────────────────────────┐
│              VisualizationPresenter::activate()                  │
│                                                                   │
│  for (sensNum = 0; sensNum < 7; ++sensNum)                      │
│  {                                                                │
│      currentT = getCurrentVal_T(sensNum);    // Читаем значение │
│      setCurrentVal_T(sensNum, currentT);     // Флаг = 1 ✅     │
│      currentH = getCurrentVal_H(sensNum);    // Читаем значение │
│      setCurrentVal_H(sensNum, currentH);     // Флаг = 1 ✅     │
│  }                                                                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ↓
┌─────────────────────────────────────────────────────────────────┐
│                          Model                                   │
│                                                                   │
│  FlagCurrentValueTChanged[7] = {1, 1, 1, 1, 1, 1, 1} ← Единицы!│
│  FlagCurrentValueHChanged[7] = {1, 1, 1, 1, 1, 1, 1} ← Единицы!│
└────────────────────────────┬────────────────────────────────────┘
                             │
                             │ ValUpdatePresenter()
                             ↓
┌─────────────────────────────────────────────────────────────────┐
│           VisualizationPresenter::ValUpdatePresenter()           │
│                                                                   │
│  for (sensNum = 0; sensNum < 7; ++sensNum)                      │
│  {                                                                │
│      if (getFlagCurrentVal_T_Chng(sensNum) == 1) // ✅ True!    │
│      {                                                            │
│          Val = getCurrentVal_T(sensNum);                         │
│          view.Val_T_X_UpdateView(Val); // Отображаем! ✨       │
│      }                                                            │
│      if (getFlagCurrentVal_H_Chng(sensNum) == 1) // ✅ True!    │
│      {                                                            │
│          Val = getCurrentVal_H(sensNum);                         │
│          view.Val_H_X_UpdateView(Val); // Отображаем! ✨       │
│      }                                                            │
│  }                                                                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ↓
┌─────────────────────────────────────────────────────────────────┐
│                    Visualization Screen                          │
│                                                                   │
│  Defroster Left:   25.0°C  65.0%  ✅ Отображено сразу!         │
│  Defroster Right:  23.0°C  68.0%  ✅ Отображено сразу!         │
│  Defroster Center: 24.0°C  70.0%  ✅ Отображено сразу!         │
│  Fish Left:        -5.0°C         ✅ Отображено сразу!         │
│  Fish Right:       -4.5°C         ✅ Отображено сразу!         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📊 Датчики на экране Visualization

| Индекс | Название | Параметры | Метод отображения T | Метод отображения H |
|--------|----------|-----------|---------------------|---------------------|
| 0 | Defroster Left | T + H | `Val_T_0UpdateView()` | `Val_H_0UpdateView()` |
| 1 | Defroster Right | T + H | `Val_T_1UpdateView()` | `Val_H_1UpdateView()` |
| 2 | Defroster Center | T + H | `Val_T_2UpdateView()` | `Val_H_2UpdateView()` |
| 3 | Fish Left | T only | `Val_T_3UpdateView()` | - |
| 4 | Fish Right | T only | `Val_T_4UpdateView()` | - |
| 5 | Defroster Body | - | - | - |
| 6 | MB IO Module | - | - | - |

---

## 🧪 Тестирование

### Тест 1: Переход на экран с главного

```
1. Находимся на экране Home
2. Нажимаем кнопку перехода на Visualization
3. ✅ РЕЗУЛЬТАТ: Все значения T/H отображаются сразу
```

### Тест 2: Переход на экран из Settings

```
1. Находимся на экране Settings
2. Нажимаем кнопку перехода на Visualization
3. ✅ РЕЗУЛЬТАТ: Все значения T/H отображаются сразу
```

### Тест 3: Возврат на экран после других экранов

```
1. Переходим: Visualization → Home → Visualization
2. ✅ РЕЗУЛЬТАТ: При каждом возврате все значения отображаются сразу
```

### Тест 4: Проверка обновления в реальном времени

```
1. Открываем экран Visualization
2. Ждём обновления данных (каждую секунду)
3. ✅ РЕЗУЛЬТАТ: Значения обновляются корректно
```

---

## ⚙️ Технические детали

### Методы Model для работы с флагами

```cpp
// Установка значения (автоматически устанавливает флаг = 1)
void Model::setCurrentVal_T(int8_t SensNumber, short Val)
{
    CurrentValueT[SensNumber] = Val;
    FlagCurrentValueTChanged[SensNumber] = 1;  // ← Флаг!
}

// Проверка флага
int8_t Model::getFlagCurrentVal_T_Chng(int8_t SensNumber)
{
    return FlagCurrentValueTChanged[SensNumber];
}

// Очистка флага (после отображения)
void Model::clearFlagCurrentVal_T_Chng(int8_t SensNumber)
{
    FlagCurrentValueTChanged[SensNumber] = 0;
}
```

### Цикл обработки во View

```cpp
// VisualizationPresenter::ValUpdatePresenter()
for (int sensNum = 0; sensNum < SQ; ++sensNum)
{
    // Проверяем флаг температуры
    if (Model::getFlagCurrentVal_T_Chng(sensNum) == 1)
    {
        short Val = Model::getCurrentVal_T(sensNum);
        Model::clearFlagCurrentVal_T_Chng(sensNum);  // Сбрасываем флаг
        
        // Вызываем метод отображения для конкретного датчика
        switch (sensNum) {
            case 0: view.Val_T_0UpdateView(Val); break;
            case 1: view.Val_T_1UpdateView(Val); break;
            // ...
        }
    }
    
    // Аналогично для влажности
    if (Model::getFlagCurrentVal_H_Chng(sensNum) == 1) { /* ... */ }
}
```

---

## 📝 Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `TouchGFX/gui/src/visualization_screen/VisualizationPresenter.cpp` | +цикл установки флагов в `activate()` |

---

## ✅ Результат

### До исправления
```
┌─────────────────────────────────────┐
│      Visualization Screen            │
│                                      │
│  Defroster Left:   --.-°C  --.-%    │  ← Пусто!
│  Defroster Right:  --.-°C  --.-%    │  ← Пусто!
│  Defroster Center: --.-°C  --.-%    │  ← Пусто!
│  Fish Left:        --.-°C            │  ← Пусто!
│  Fish Right:       --.-°C            │  ← Пусто!
│                                      │
│  (через 1 сек значения появятся)    │
└─────────────────────────────────────┘
```

### После исправления
```
┌─────────────────────────────────────┐
│      Visualization Screen            │
│                                      │
│  Defroster Left:   25.0°C  65.0%    │  ✅ Сразу!
│  Defroster Right:  23.0°C  68.0%    │  ✅ Сразу!
│  Defroster Center: 24.0°C  70.0%    │  ✅ Сразу!
│  Fish Left:        -5.0°C            │  ✅ Сразу!
│  Fish Right:       -4.5°C            │  ✅ Сразу!
│                                      │
│  (все значения видны немедленно!)    │
└─────────────────────────────────────┘
```

---

## 🎯 Аналогичная проблема на других экранах?

Если на других экранах (Home, Settings) тоже не отображаются значения сразу, примените то же решение:

```cpp
void <Screen>Presenter::activate()
{
    // Установить флаги для всех нужных значений
    for (int sensNum = 0; sensNum < SQ; ++sensNum)
    {
        Model::setCurrentVal_T(sensNum, Model::getCurrentVal_T(sensNum));
        Model::setCurrentVal_H(sensNum, Model::getCurrentVal_H(sensNum));
    }
    
    // Вызвать обновление
    <Screen>Presenter::ValUpdatePresenter();
}
```

---

## 📚 Связанные документы

- [Model.hpp](../TouchGFX/gui/include/gui/model/Model.hpp) - Определение флагов и методов
- [Model.cpp](../TouchGFX/gui/src/model/Model.cpp) - Реализация методов работы с флагами

---

**Проблема решена!** ✅  
**Дата**: 12 ноября 2025  
**Версия прошивки**: 1.1.0  
**Автор**: AI Assistant (Claude)

