#ifndef SINGLETON_H
#define SINGLETON_H

#include <utility>

template <typename T>
class Singleton {
   protected:
    Singleton() = default;
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton& operator=(Singleton&&) = delete;
    virtual ~Singleton() = default;

   public:
    template <typename... Args>
    static T* instance(Args... args) {
        static T inst{std::forward<Args>(args)...};
        return &inst;
    }
};

#endif // SINGLETON_H
