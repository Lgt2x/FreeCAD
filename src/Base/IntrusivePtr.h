// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef BASE_INTRUSIVEPTR_H
#define BASE_INTRUSIVEPTR_H

#include <cstddef>
#include <type_traits>
#include <utility>

namespace Base
{

template<class T>
class IntrusivePtr
{
public:
    using element_type = T;

    constexpr IntrusivePtr() noexcept = default;
    constexpr IntrusivePtr(std::nullptr_t) noexcept
    {}

    IntrusivePtr(T* ptr, bool addRef = true)
        : ptr_(ptr)
    {
        if (ptr_ && addRef) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    IntrusivePtr(const IntrusivePtr& other)
        : ptr_(other.ptr_)
    {
        if (ptr_) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    template<class U>
    IntrusivePtr(const IntrusivePtr<U>& other) requires(std::is_convertible_v<U*, T*>)
        : ptr_(other.ptr_)
    {
        if (ptr_) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    IntrusivePtr(IntrusivePtr&& other) noexcept
        : ptr_(other.detach())
    {}

    template<class U>
    IntrusivePtr(IntrusivePtr<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>)
        : ptr_(other.detach())
    {}

    ~IntrusivePtr()
    {
        if (ptr_) {
            intrusive_ptr_release(ptr_);
        }
    }

    IntrusivePtr& operator=(const IntrusivePtr& other)
    {
        if (this == &other) {
            return *this;
        }
        reset(other.ptr_);
        return *this;
    }

    template<class U>
    IntrusivePtr& operator=(const IntrusivePtr<U>& other) requires(std::is_convertible_v<U*, T*>)
    {
        reset(other.ptr_);
        return *this;
    }

    IntrusivePtr& operator=(IntrusivePtr&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        if (ptr_) {
            intrusive_ptr_release(ptr_);
        }
        ptr_ = other.detach();
        return *this;
    }

    template<class U>
    IntrusivePtr& operator=(IntrusivePtr<U>&& other) noexcept requires(std::is_convertible_v<U*, T*>)
    {
        if (ptr_) {
            intrusive_ptr_release(ptr_);
        }
        ptr_ = other.detach();
        return *this;
    }

    void reset() noexcept
    {
        if (ptr_) {
            intrusive_ptr_release(ptr_);
            ptr_ = nullptr;
        }
    }

    void reset(T* ptr, bool addRef = true)
    {
        if (ptr == ptr_) {
            return;
        }
        if (ptr_ != nullptr) {
            intrusive_ptr_release(ptr_);
        }
        ptr_ = ptr;
        if (ptr_ && addRef) {
            intrusive_ptr_add_ref(ptr_);
        }
    }

    T* get() const noexcept
    {
        return ptr_;
    }

    T& operator*() const noexcept
    {
        return *ptr_;
    }

    T* operator->() const noexcept
    {
        return ptr_;
    }

    explicit operator bool() const noexcept
    {
        return ptr_ != nullptr;
    }

    void swap(IntrusivePtr& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
    }

private:
    template<class U>
    friend class IntrusivePtr;

    T* detach() noexcept
    {
        return std::exchange(ptr_, nullptr);
    }

    T* ptr_ = nullptr;
};

template<class T, class U>
inline bool operator==(const IntrusivePtr<T>& a, const IntrusivePtr<U>& b) noexcept
{
    return a.get() == b.get();
}

template<class T, class U>
inline bool operator!=(const IntrusivePtr<T>& a, const IntrusivePtr<U>& b) noexcept
{
    return a.get() != b.get();
}

template<class T, class U>
inline bool operator==(const IntrusivePtr<T>& a, U* b) noexcept
{
    return a.get() == b;
}

template<class T, class U>
inline bool operator!=(const IntrusivePtr<T>& a, U* b) noexcept
{
    return a.get() != b;
}

template<class T, class U>
inline bool operator==(U* a, const IntrusivePtr<T>& b) noexcept
{
    return a == b.get();
}

template<class T, class U>
inline bool operator!=(U* a, const IntrusivePtr<T>& b) noexcept
{
    return a != b.get();
}

}  // namespace Base

#endif  // BASE_INTRUSIVEPTR_H

