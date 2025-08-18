#pragma once
#include <QVector>
#include <algorithm>

class StatsCalculator {
public:
    void setMaxSamples(int n) { max_ = n; trim_(); }
    void clear() { samples_.clear(); }

    void addSample(qint64 ms) {
        if (ms < 0) return;
        samples_.push_back(ms);
        trim_();
    }

    bool empty() const { return samples_.isEmpty(); }

    qint64 min() const {
        if (samples_.isEmpty()) return -1;
        return *std::min_element(samples_.cbegin(), samples_.cend());
    }
    qint64 max() const {
        if (samples_.isEmpty()) return -1;
        return *std::max_element(samples_.cbegin(), samples_.cend());
    }
    qint64 avg() const {
        if (samples_.isEmpty()) return -1;
        long long sum = 0; for (auto v : samples_) sum += v; return sum / samples_.size();
    }
    int count() const { return samples_.size(); }

private:
    void trim_() {
        while (samples_.size() > max_) samples_.erase(samples_.begin());
    }

    QVector<qint64> samples_;
    int max_ = 50;
};