#pragma once

namespace Net
{
    template <typename... Argtypes>
    class Callback_interface
    {
    public:
        virtual ~Callback_interface() = default;
        virtual void invoke(Argtypes... args) = 0;
    };
}
