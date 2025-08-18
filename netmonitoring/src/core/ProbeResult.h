#pragma once
#include <QString>
#include <optional>

struct ProbeResult {
    enum class Status { Up, Down, DnsFail, Timeout, Error };

    Status status = Status::Error;
    qint64 latencyMs = -1;                // время до успеха/ответа
    qint64 dnsMs = -1;                    // длительность DNS-резолва (если замерялась)
    std::optional<int> httpCode;          // для HTTP-режима
    QString ip;                           // выбранный IP (если известен)
    QString message;                      // текст ошибки/детали
};
