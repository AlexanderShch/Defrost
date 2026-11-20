#pragma once
#include <stdint.h>
#include <string.h>

// Поля события диагностики передаются little-endian на STM32 и Windows x64.
#pragma pack(push, 1)
struct SensorEventDetail {
    uint8_t index, address, function, reason, expected, received, uart, sampleSize;
    uint16_t elapsedMs;
    uint32_t attempts, durationSec;
    uint8_t sample[9];
};
#pragma pack(pop)
static_assert(sizeof(SensorEventDetail) == 27, "event wire format");

class ButtonEventQueue {
    struct Event {
        uint32_t sequence, seconds;
        uint8_t kind;
        SensorEventDetail detail;
    };
    static constexpr unsigned capacity = 16, reservedButtons = 4;
    Event events[capacity] = {};
    unsigned count = 0, diagnostics = 0;
    uint32_t next = 1, offered = 0, legacyOffered = 0;
    uint32_t lostButtons = 0, lostDiagnostics = 0;
    uint8_t session[16] = {};
    bool bound = false;

    void Bind(const uint8_t* proposed) {
        if (!bound) { memcpy(session, proposed, 16); bound = true; }
    }
    static void Encode(const Event& e, uint8_t* dest) {
        memcpy(dest, &e.sequence, 4);
        memcpy(dest + 4, &e.seconds, 4);
        dest[8] = e.kind;
    }
    void Remove(unsigned i) {
        if (events[i].kind > 2) --diagnostics;
        --count;
        memmove(events+i, events+i+1, (count-i)*sizeof(Event));
    }
public:
    bool Pending() const { return count != 0; }

    void Push(uint8_t kind, uint32_t seconds) {
        if (count == capacity) { ++lostButtons; return; }
        events[count++] = {next++, seconds, kind, {}};
    }
    void PushDiagnostic(uint8_t kind, uint32_t seconds, const SensorEventDetail& detail) {
        if (count == capacity || diagnostics >= capacity-reservedButtons) {
            ++lostDiagnostics;
            return;
        }
        events[count++] = {next++, seconds, kind, detail};
        ++diagnostics;
    }

    // Старый сервер видит только кнопки. Его ACK не удаляет диагностику.
    uint8_t Read(const uint8_t* proposed, uint8_t* out) {
        Bind(proposed);
        memcpy(out, session, 16);
        memcpy(out+16, &lostButtons, 4);
        uint8_t n = 0;
        for (unsigned i=0; i<count && n<10; ++i) {
            if (events[i].kind > 2) continue;
            Encode(events[i], out+21+9*n++);
            legacyOffered = events[i].sequence;
        }
        out[20] = n;
        return 21+9*n;
    }
    bool Ack(const uint8_t* id, uint32_t seq) {
        if (!bound || memcmp(session,id,16) || seq>legacyOffered) return false;
        for (unsigned i=0; i<count && events[i].sequence<=seq;) {
            if (events[i].kind<=2) Remove(i);
            else ++i;
        }
        return true;
    }
    // Заголовок: сессия, потери кнопок и диагностики, число ожидающих.
    // Одна запись за чтение; максимум 25+9+27 байт полезной нагрузки.
    uint8_t ReadEvent(const uint8_t* proposed, uint8_t* out) {
        Bind(proposed);
        memcpy(out, session, 16);
        memcpy(out+16, &lostButtons, 4);
        memcpy(out+20, &lostDiagnostics, 4);
        out[24] = count;
        if (!count) return 25;
        const Event& e = events[0];
        Encode(e,out+25);
        offered = e.sequence;
        if (e.kind <= 2) return 34;
        memcpy(out+34, &e.detail, sizeof(e.detail));
        return 34+sizeof(e.detail);
    }
    bool AckEvent(const uint8_t* id, uint32_t seq) {
        if (!bound || memcmp(session,id,16) || seq==0 || seq>offered) return false;
        if (count && events[0].sequence==seq) Remove(0);
        return true;
    }
};
extern ButtonEventQueue g_buttonEvents;
