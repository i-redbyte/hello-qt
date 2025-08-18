#include "MonitorController.h"
#include "TcpConnectProbe.h"
#include "HttpHeadProbe.h"

MonitorController::MonitorController(QObject* parent)
        : QObject(parent) {
    intervalTimer_.setSingleShot(false);
    QObject::connect(&intervalTimer_, &QTimer::timeout, this, &MonitorController::runProbe_);
    stats_.setMaxSamples(50);
}

void MonitorController::setMode(Mode m) { mode_ = m; }

void MonitorController::setTarget(QString host, quint16 port) {
    host_ = std::move(host);
    port_ = port;
}

void MonitorController::setIntervalSec(int sec) {
    intervalMs_ = qMax(1, sec) * 1000;
    if (intervalTimer_.isActive()) {
        intervalTimer_.start(intervalMs_);
    }
}

void MonitorController::setTimeoutMs(int ms) { timeoutMs_ = ms; }

void MonitorController::setMaxSamples(int n) { stats_.setMaxSamples(n); }

void MonitorController::start() {
    if (intervalTimer_.isActive()) return;
    intervalTimer_.start(intervalMs_);
    runProbe_();
}

void MonitorController::stop() {
    intervalTimer_.stop();
}

void MonitorController::checkOnce() {
    runProbe_();
}

void MonitorController::runProbe_() {
    if (probing_) return;
    probing_ = true;
    emit probeStarted();

    probe_ = makeProbe_();

    QObject::connect(probe_.get(), &INetProbe::progressDnsResolved, this,
                     &MonitorController::probeProgressDns);

    QObject::connect(probe_.get(), &INetProbe::finished, this,
                     [this](const ProbeResult& r){ onProbeFinished_(r); });

    probe_->start(host_, port_, timeoutMs_);
}

void MonitorController::onProbeFinished_(const ProbeResult& r) {
    probing_ = false;

    if (r.latencyMs >= 0) {
        stats_.addSample(r.latencyMs);
        emit statsUpdated(stats_.min(), stats_.avg(), stats_.max(), stats_.count());
    }

    emit probeFinished(r);

    probe_.reset();
}

std::unique_ptr<INetProbe> MonitorController::makeProbe_() const {
    switch (mode_) {
        case Mode::TcpConnect:
            return std::make_unique<TcpConnectProbe>();
        case Mode::HttpHead:
            return std::make_unique<HttpHeadProbe>();
    }
    return std::make_unique<TcpConnectProbe>();
}