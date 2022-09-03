module;
#include <mutex>

export module Network_framework:Thread_safe_deque;

export namespace Net
{
    template <typename T>
    class Thread_safe_deque
    {
    public:
        Thread_safe_deque() noexcept = default;
        Thread_safe_deque(const Thread_safe_deque&) = delete;
        Thread_safe_deque(Thread_safe_deque&&) = delete;
        ~Thread_safe_deque()
        {
            clear();
        }

        Thread_safe_deque& operator=(const Thread_safe_deque&) = delete;
        Thread_safe_deque& operator=(Thread_safe_deque&&) = delete;

        [[nodiscard]] const T& front()
        {
           
        }

        [[nodiscard]] const T& back()
        {
            
        }

        void push_front(T item)
        {
            
        }

        void push_back(T item)
        {
            
        }

        [[nodiscard]] bool empty()
        {
           
        }

        template <typename... Argtypes>
        void erase(const Argtypes&... args)
        {
          
        }

        [[nodiscard]] size_t size()
        {
            return 0;
        }

        void clear()
        {
            
        }

        T pop_front()
        {
            return T();
        }

        T pop_back()
        {
            return T();
        }

    private:
        std::mutex m_mutex;
       
    };
} // namespace Net
