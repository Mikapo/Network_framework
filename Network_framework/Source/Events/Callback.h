#pragma once

#include "Callback_interface.h"
#include <functional>

namespace Net
{
    template <typename Callable, typename... Argtypes>
    class Callback : public Callback_interface<Argtypes...>
    {
    public:
        Callback(Callable callable) noexcept : m_callback(std::move(callable))
        {
        }

        void invoke(Argtypes... args) override
        {
            std::invoke(m_callback, std::forward<Argtypes>(args)...);
        }

    private:
        Callable m_callback;
    };
} // namespace Net
