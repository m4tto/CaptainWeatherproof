// sma.hpp
#ifndef SMA_HPP
#define SMA_HPP

#include <vector>
#include <cstddef>
#include <stdexcept>

class SimpleMovingAverage {
public:
    /// Construct an SMA that averages over the last 'window' samples.
    explicit SimpleMovingAverage(std::size_t window)
        : buf_(window, 0.0),
          window_(window),
          count_(0),
          idx_(0),
          sum_(0.0)
    {
        if (window_ == 0) {
            throw std::invalid_argument("Window size must be > 0");
        }
    }

    /// Feed a new sample, return the updated average.
    double update(double sample) {
        if (count_ < window_) {
            // still filling buffer
            sum_        += sample;
            buf_[idx_]  = sample;
            ++count_;
        } else {
            // buffer full: subtract oldest, add newest
            sum_        += sample - buf_[idx_];
            buf_[idx_]   = sample;
        }
        // advance circular index
        idx_ = (idx_ + 1) % window_;
        return sum_ / static_cast<double>(count_);
    }

    double get() {
        return count_ != 0 ? sum_ / static_cast<double>(count_) : 0.0;
    }

    /// Reset the filter (clears history)
    void reset() {
        std::fill(buf_.begin(), buf_.end(), 0.0);
        count_ = 0;
        idx_   = 0;
        sum_   = 0.0;
    }

    /// Get current window size
    std::size_t window() const { return window_; }

    /// Get number of samples seen so far (<= window)
    std::size_t count()  const { return count_; }

private:
    std::vector<double> buf_;
    std::size_t         window_;
    std::size_t         count_;
    std::size_t         idx_;
    double              sum_;
};

#endif // SMA_HPP