// RainStats.hpp
#ifndef RAIN_STATS_HPP
#define RAIN_STATS_HPP

#include <cstdint>
#include <cmath>

// Vevor 7-in-1: the rain counter is a 16-bit tip count (offset 0x101 removed
// by the decoder) scaled by 0.233 mm/tip. Working in integer tips avoids float
// drift and makes "same value" comparisons exact.
constexpr float    RAIN_MM_PER_TIP          = 0.233f;
constexpr int32_t  RAIN_MAX_TIPS            = 0xFFFF - 0x101;
// Physical cap on rain intensity used to reject impossible jumps (mm/h).
constexpr float    RAIN_MAX_MM_PER_HOUR     = 300.0f;
// Readings closer than this belong to the same RF burst (the station repeats
// the frame inside one transmission), so they cannot confirm each other.
constexpr uint32_t RAIN_SAME_BURST_MS       = 5000;
// Consecutive, mutually consistent transmissions needed to (re)baseline the
// counter after boot, a sensor reset/battery swap or a 16-bit wrap.
constexpr uint8_t  RAIN_REBASE_CONFIRM      = 3;

// Validates the raw cumulative counter. The 8-bit additive checksum of the
// protocol lets corrupted frames through, so a value is only accepted when a
// later, independent transmission agrees with it:
//   - a new value must be >= the accepted one and within the max rain rate;
//   - it becomes a candidate and is accepted only when a reading from a
//     different burst is consistent with it (same or slightly higher);
//   - anything else is ignored, unless RAIN_REBASE_CONFIRM consecutive
//     transmissions agree on it, in which case the counter is re-baselined
//     with a zero delta (never a spike).
class RainCounterFilter {
public:
    // Returns the confirmed rain increment in tips (always >= 0).
    int32_t update(uint32_t id, int32_t raw, uint32_t nowMs) {
        if (raw < 0 || raw > RAIN_MAX_TIPS)
            return 0;

        if (!initialized_) {
            if (trackRebase(id, raw, nowMs))
                rebase(id, raw, nowMs);
            return 0;
        }

        if (id == id_ && raw == accepted_) {
            acceptedMs_   = nowMs;
            hasCandidate_ = false;
            rebaseCount_  = 0;
            return 0;
        }

        if (id == id_ && plausible(accepted_, raw, nowMs - acceptedMs_)) {
            rebaseCount_ = 0;

            if (hasCandidate_ && nowMs - candidateMs_ < RAIN_SAME_BURST_MS) {
                // Second copy of the same burst: if the copies disagree,
                // trust neither.
                if (raw != candidate_)
                    hasCandidate_ = false;
                return 0;
            }

            if (hasCandidate_ && plausible(candidate_, raw, nowMs - candidateMs_)) {
                int32_t delta = candidate_ - accepted_;
                accepted_   = candidate_;
                acceptedMs_ = candidateMs_;
                if (raw > candidate_) {
                    candidate_   = raw;
                    candidateMs_ = nowMs;
                } else {
                    hasCandidate_ = false;
                    acceptedMs_   = nowMs;
                }
                return delta;
            }

            candidate_    = raw;
            candidateMs_  = nowMs;
            hasCandidate_ = true;
            return 0;
        }

        // Implausible w.r.t. the accepted value: either garbage or a real
        // counter discontinuity. Only the latter repeats consistently.
        if (trackRebase(id, raw, nowMs))
            rebase(id, raw, nowMs);
        return 0;
    }

    bool     initialized() const { return initialized_; }
    int32_t  accepted()    const { return accepted_; }
    float    acceptedMm()  const { return accepted_ * RAIN_MM_PER_TIP; }
    uint32_t rebaseEvents() const { return rebaseEvents_; }

private:
    static int32_t maxStep(uint32_t dtMs) {
        if (dtMs > 3600000UL) dtMs = 3600000UL;
        const float tipsPerMs = RAIN_MAX_MM_PER_HOUR / RAIN_MM_PER_TIP / 3600000.0f;
        // +2 tips of slack for the reading-time jitter at low rates.
        return 2 + static_cast<int32_t>(std::ceil(dtMs * tipsPerMs));
    }

    static bool plausible(int32_t from, int32_t to, uint32_t dtMs) {
        return to >= from && to - from <= maxStep(dtMs);
    }

    bool trackRebase(uint32_t id, int32_t raw, uint32_t nowMs) {
        if (rebaseCount_ > 0 && nowMs - rebaseMs_ < RAIN_SAME_BURST_MS) {
            if (id != rebaseId_ || raw != rebaseValue_)
                rebaseCount_ = 0;
            return false;
        }
        if (rebaseCount_ > 0 && id == rebaseId_ &&
            plausible(rebaseValue_, raw, nowMs - rebaseMs_))
            rebaseCount_++;
        else
            rebaseCount_ = 1;

        rebaseId_    = id;
        rebaseValue_ = raw;
        rebaseMs_    = nowMs;
        return rebaseCount_ >= RAIN_REBASE_CONFIRM;
    }

    void rebase(uint32_t id, int32_t raw, uint32_t nowMs) {
        if (initialized_)
            rebaseEvents_++;
        initialized_  = true;
        id_           = id;
        accepted_     = raw;
        acceptedMs_   = nowMs;
        hasCandidate_ = false;
        rebaseCount_  = 0;
    }

    bool     initialized_  = false;
    uint32_t id_           = 0;
    int32_t  accepted_     = 0;
    uint32_t acceptedMs_   = 0;

    bool     hasCandidate_ = false;
    int32_t  candidate_    = 0;
    uint32_t candidateMs_  = 0;

    uint8_t  rebaseCount_  = 0;
    uint32_t rebaseId_     = 0;
    int32_t  rebaseValue_  = 0;
    uint32_t rebaseMs_     = 0;

    uint32_t rebaseEvents_ = 0;
};

// Minute/hour/day accumulators fed only by confirmed increments.
// Buckets are keyed on wall-clock time and only advance once NTP is synced.
struct RainStats {
    RainCounterFilter filter;

    int32_t minuteTips = 0;
    int32_t hourTips   = 0;
    int32_t dayTips    = 0;

    uint32_t lastMinute = 0;
    uint32_t lastHour   = 0;
    uint32_t lastDay    = 0;

    // Rolling last-60-minutes window (Wunderground "rainin"): one bin per
    // wall-clock minute, stamped with its minute so stale bins are skipped.
    int32_t  binTips[60]   = {};
    uint32_t binMinute[60] = {};

    static constexpr uint32_t MIN_VALID_EPOCH = 1704067200UL; // 2024-01-01

    void update(uint32_t id, float rainMm, uint32_t nowMs, uint32_t epoch) {
        int32_t raw = static_cast<int32_t>(std::lround(rainMm / RAIN_MM_PER_TIP));
        int32_t delta = filter.update(id, raw, nowMs);
        roll(epoch);
        minuteTips += delta;
        hourTips   += delta;
        dayTips    += delta;

        if (epoch >= MIN_VALID_EPOCH) {
            uint32_t m = epoch / 60;
            uint8_t  i = m % 60;
            if (binMinute[i] != m) { binMinute[i] = m; binTips[i] = 0; }
            binTips[i] += delta;
        }
    }

    int32_t last60MinTips(uint32_t epoch) const {
        if (epoch < MIN_VALID_EPOCH)
            return 0;
        uint32_t m = epoch / 60;
        int32_t sum = 0;
        for (uint8_t i = 0; i < 60; i++)
            if (binMinute[i] <= m && m - binMinute[i] < 60)
                sum += binTips[i];
        return sum;
    }

    // Resets the buckets whose period has elapsed.
    void roll(uint32_t epoch) {
        if (epoch < MIN_VALID_EPOCH)
            return;
        uint32_t m = epoch / 60, h = epoch / 3600, d = epoch / 86400;
        if (m != lastMinute) { minuteTips = 0; lastMinute = m; }
        if (h != lastHour)   { hourTips   = 0; lastHour   = h; }
        if (d != lastDay)    { dayTips    = 0; lastDay    = d; }
    }

    float minuteMm() const { return minuteTips * RAIN_MM_PER_TIP; }
    float hourMm()   const { return hourTips   * RAIN_MM_PER_TIP; }
    float dayMm()    const { return dayTips    * RAIN_MM_PER_TIP; }
    float last60MinMm(uint32_t epoch) const { return last60MinTips(epoch) * RAIN_MM_PER_TIP; }
};

#endif // RAIN_STATS_HPP
