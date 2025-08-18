#pragma once
#include <QObject>
#include <QString>
#include "ProbeResult.h"

class INetProbe : public QObject {
    Q_OBJECT
public:
    explicit INetProbe(QObject* parent = nullptr) : QObject(parent) {}
    ~INetProbe() override = default;

    virtual void start(const QString& host, quint16 port, int timeoutMs) = 0;
    virtual void abort() = 0;

    signals:
            void finished(const ProbeResult& result);
    void progressDnsResolved(qint64 dnsMs, const QString& ip);
};
