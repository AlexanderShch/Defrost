#pragma once
#include <stdint.h>
#include <string.h>

// Очередь живёт в RAM до перезапуска питания. Доступ из задач опроса и команд
// сериализуется osKernelLock вызывающей стороной; аппаратные прерывания её не используют.
class ButtonEventQueue {
    struct Event { uint32_t sequence, seconds; uint8_t button; };
    Event events[512] = {};
    uint16_t head = 0, count = 0;
    uint32_t next = 1, offered = 0, overflow = 0;
    uint8_t session[16] = {};
    bool bound = false;
public:
    // Сохраняет обработанное физическое нажатие (1=СТАРТ, 2=СТОП).
    // Переполнение не вытесняет неподтверждённые события и явно учитывается.
    void Push(uint8_t button, uint32_t seconds) {
        if (count == 512) { ++overflow; return; }
        events[(head + count) % 512] = {next++, seconds, button};
        ++count;
    }
    // Первый сервер задаёт случайный идентификатор загрузки. Повторное подключение
    // не меняет его; чтение не удаляет события. Формат: session[16], overflow[4],
    // count[1], затем до 10 записей sequence[4], seconds[4], button[1], little-endian.
    uint8_t Read(const uint8_t* proposedSession, uint8_t* out) {
        if (!bound) { memcpy(session, proposedSession, 16); bound = true; }
        memcpy(out, session, 16); memcpy(out + 16, &overflow, 4);
        const uint8_t n = count > 10 ? 10 : (uint8_t)count;
        out[20] = n;
        for (uint8_t i=0; i<n; ++i) {
            const Event& e=events[(head+i)%512];
            memcpy(out+21+9*i, &e.sequence, 4);
            memcpy(out+25+9*i, &e.seconds, 4); out[29+9*i]=e.button;
            offered=e.sequence;
        }
        return (uint8_t)(21+9*n);
    }
    // Подтверждает только ранее предложенный префикс той же загрузки.
    // Повторный ACK безопасен; ACK из старого соединения не удаляет новые события.
    bool Ack(const uint8_t* id, uint32_t sequence) {
        if (!bound || memcmp(session,id,16)!=0 || sequence>offered) return false;
        while (count && events[head].sequence<=sequence) {head=(head+1)%512;--count;}
        return true;
    }
};

extern ButtonEventQueue g_buttonEvents;
