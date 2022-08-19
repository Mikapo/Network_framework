#pragma once

#include <functional>

namespace Net
{
    template <typename... Parameter_types>
    class Delegate
    {
    public:
        template <typename Func_type>
        void set_function(const Func_type& func) noexcept
        {
            m_callback = func;
        }

        [[nodiscard]] bool function_has_been_set() const noexcept
        {
            return m_callback.operator bool();
        }

        bool broadcast(Parameter_types... parameters) const
        {
            if (!function_has_been_set())
                return false;

            m_callback(std::forward<Parameter_types>(parameters)...);
            return true;
        }

    private:
        std::function<void(Parameter_types...)> m_callback;
    };
} // namespace Net
