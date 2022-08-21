#pragma once

#include <functional>

namespace Net
{
    // The class that handles callbacks
    template <typename... Parameter_types>
    class Delegate
    {
    public:
        /**
        *   Sets the callable that gets called when this delegate is broadcasted
        * 
        *   @param any callable with correct parameter types and void return type
        */
        template <typename Callable_type>
        void set_callback(Callable_type callable)
        {
            m_callback = std::forward<Callable_type>(callable);
        }

        [[nodiscard]] bool has_been_set() const noexcept
        {
            return m_callback.operator bool();
        }

        /**
        *   Triggers the callback set on this delegate
        * 
        *   @param the parameters to forward into the callback
        *   @return has callback been set for this delegate
        */
        bool broadcast(Parameter_types... parameters) const
        {
            if (!has_been_set())
                return false;

            m_callback(std::forward<Parameter_types>(parameters)...);
            return true;
        }

    private:
        std::function<void(Parameter_types...)> m_callback;
    };
} // namespace Net
