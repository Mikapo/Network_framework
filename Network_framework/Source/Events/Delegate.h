#pragma once

#include "Callback.h"
#include "Object_Callback.h"
#include <memory>

namespace Net
{
    // The class that handles callbacks
    template <typename... Parameter_types>
    class Delegate
    {
    public:
        using Callback_interface = std::unique_ptr<Callback_interface<Parameter_types...>>;

        /**
         *   Sets the callable that gets called when this delegate is broadcasted
         *
         *   @param any callable with correct parameter types and void return type
         */
        template <typename Callable_type>
        void set_callback(Callable_type callable)
        {
            using Callback_type = Callback<Callable_type, Parameter_types...>;

            m_callback = std::make_unique<Callback_type>(std::forward<Callable_type>(callable));
        }

        template <typename Obj, typename Callable_type>
        void set_callback(Obj* obj, Callable_type callable)
        {
            using Callback_type = Object_callback<Obj, Callable_type, Parameter_types...>;

            m_callback = std::make_unique<Callback_type>(obj, std::forward<Callable_type>(callable));
        }

        [[nodiscard]] bool has_been_set() const noexcept
        {
            return m_callback != nullptr;
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

            m_callback->invoke(std::forward<Parameter_types>(parameters)...);
            return true;
        }

    private:
        Callback_interface m_callback;
    };
} // namespace Net
