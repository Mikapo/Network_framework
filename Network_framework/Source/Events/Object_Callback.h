#pragma once

#include "Callback_interface.h"

namespace Net
{
	template<typename Obj, typename Callable, typename... Argtypes>
	class Object_callback : public Callback_interface<Argtypes...>
    {
	public:
        Object_callback(Obj* obj, Callable callable)
            : m_obj(obj), m_callback(std::forward<Callable>(callable))
        {
        }

        void invoke(Argtypes... args) override
        {
            std::invoke(m_callback, m_obj, std::forward<Argtypes>(args)...);
        }

    private:
		Obj* m_obj;
        Callable m_callback;
	};
}