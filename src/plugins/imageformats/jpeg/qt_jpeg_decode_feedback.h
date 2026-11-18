// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <functional>
#include <utility>

namespace QtJpegFeedback {
inline thread_local const std::function<bool()> *cancellation = nullptr;

class CancellationScope
{
public:
    explicit CancellationScope(const std::function<bool()> &cancelled)
        : previous(std::exchange(cancellation, &cancelled))
    {
    }

    ~CancellationScope() { cancellation = previous; }
    CancellationScope(const CancellationScope &) = delete;
    CancellationScope &operator=(const CancellationScope &) = delete;

private:
    const std::function<bool()> *const previous;
};

inline bool cancelled()
{
    return cancellation && *cancellation && (*cancellation)();
}
}
