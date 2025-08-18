#include "TcpConnectProbe.h"
#include <QHostAddress>

TcpConnectProbe::TcpConnectProbe(QObject* parent)
        : INetProbe(parent) {
    timeout_.setSingleShot(true);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    QObject::connect(&socket_, &QTcpSocket::connected, this, [this]{
        if (!active_) return;
        ProbeResult r;
        r.status = ProbeResult::Status::Up;
        r.latencyMs = elapsed_.elapsed();
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });

    QObject::connect(&socket_, &QTcpSocket::errorOccurred, this,
                     [this](QAbstractSocket::SocketError){
        if (!active_) return;
        ProbeResult r;
        r.status = ProbeResult::Status::Down;
        r.message = socket_.errorString();
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });
#else
    QObject::connect(&socket_, SIGNAL(connected()), this, [this]{
        if (!active_) return;
        ProbeResult r;
        r.status = ProbeResult::Status::Up;
        r.latencyMs = elapsed_.elapsed();
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });
    QObject::connect(&socket_, SIGNAL(error(QAbstractSocket::SocketError)), this, [this]{
        if (!active_) return;
        ProbeResult r;
        r.status = ProbeResult::Status::Down;
        r.message = socket_.errorString();
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });
#endif

    QObject::connect(&timeout_, &QTimer::timeout, this, [this]{
        if (!active_) return;
        ProbeResult r;
        r.status = ProbeResult::Status::Timeout;
        r.message = tr("Timeout");
        r.dnsMs = dnsElapsed_.isValid() ? dnsElapsed_.elapsed() : -1;
        r.ip = ip_;
        finish_(r);
    });
}

TcpConnectProbe::~TcpConnectProbe() {
    abort();
}

void TcpConnectProbe::start(const QString& host, quint16 port, int timeoutMs) {
    if (active_) return;
    host_ = host;
    port_ = port;
    timeoutMs_ = timeoutMs;

    active_ = true;
    ip_.clear();
    elapsed_.restart();
    dnsElapsed_.restart();

    timeout_.start(timeoutMs_);

    QPointer<TcpConnectProbe> self(this);
    QHostInfo::lookupHost(host_, this, [this, self](const QHostInfo& info){
        if (!self || !active_) return;
        if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
            ProbeResult r;
            r.status = ProbeResult::Status::DnsFail;
            r.message = info.errorString();
            r.dnsMs = -1;
            finish_(r);
            return;
        }
        QHostAddress addr;
        for (const auto& a : info.addresses()) {
            if (a.protocol() == QAbstractSocket::IPv4Protocol) { addr = a; break; }
        }
        if (addr.isNull()) addr = info.addresses().first();
        ip_ = addr.toString();
        emit progressDnsResolved(dnsElapsed_.elapsed(), ip_);

        socket_.abort();
        socket_.connectToHost(addr, port_);
    });
}

void TcpConnectProbe::abort() {
    if (!active_) return;
    active_ = false;
    timeout_.stop();
    socket_.abort();
}

void TcpConnectProbe::finish_(const ProbeResult& r) {
    active_ = false;
    timeout_.stop();
    socket_.abort();
    emit finished(r);
}