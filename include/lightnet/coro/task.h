#pragma once



#include <coroutine>

#include <exception>

#include <memory>

#include <utility>



namespace lightnet {



/// @brief C++20 协程 Task 类型前向声明

template<typename T>

class Task;



/// @brief 无返回值协程 Task<void>

/// 用于连接处理等 fire-and-forget 场景，支持 co_await 嵌套调用

template<>

class Task<void> {

public:

    /// @brief 协程 promise 类型，编译器生成状态机

    struct promise_type {

        /// @brief 创建 Task 对象并绑定 coroutine_handle

        Task get_return_object() {

            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};

        }

        /// @brief 协程创建后立即挂起，需 start() 或 co_await 才运行

        std::suspend_always initial_suspend() noexcept { return {}; }

        /// @brief 协程结束时恢复 continuation（调用者协程）

        auto final_suspend() noexcept {

            struct Awaiter {

                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {

                    if (h.promise().continuation_) {

                        return h.promise().continuation_;

                    }

                    return std::noop_coroutine();

                }

                void await_resume() noexcept {}

            };

            return Awaiter{};

        }

        /// @brief 捕获未处理异常

        void unhandled_exception() { exception_ = std::current_exception(); }

        void return_void() {}



        std::coroutine_handle<> continuation_;  ///< 等待本 Task 完成的父协程

        std::exception_ptr exception_;        ///< 协程内抛出的异常

    };



    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}



    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {

        if (this != &other) {

            if (handle_) handle_.destroy();

            handle_ = std::exchange(other.handle_, {});

        }

        return *this;

    }



    ~Task() {

        if (handle_) handle_.destroy();

    }



    Task(const Task&) = delete;

    Task& operator=(const Task&) = delete;



    /// @brief 使 Task 可被 co_await，实现协程链式调用

    auto operator co_await() {

        struct Awaiter {

            bool await_ready() noexcept { return handle_.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {

                handle_.promise().continuation_ = cont;

                return handle_;

            }

            void await_resume() {

                if (handle_.promise().exception_) {

                    std::rethrow_exception(handle_.promise().exception_);

                }

            }

            std::coroutine_handle<promise_type> handle_;

        };

        return Awaiter{handle_};

    }



    /// @brief 手动启动协程（首次 resume）

    void start() {

        if (handle_) handle_.resume();

    }



    /// @brief 协程是否已执行完毕

    bool done() const { return !handle_ || handle_.done(); }



private:

    std::coroutine_handle<promise_type> handle_;  ///< 协程句柄

};



/// @brief 带返回值 T 的协程 Task

template<typename T>

class Task {

public:

    struct promise_type {

        Task get_return_object() {

            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};

        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        auto final_suspend() noexcept {

            struct Awaiter {

                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {

                    if (h.promise().continuation_) return h.promise().continuation_;

                    return std::noop_coroutine();

                }

                void await_resume() noexcept {}

            };

            return Awaiter{};

        }

        void unhandled_exception() { exception_ = std::current_exception(); }

        /// @brief 保存 co_return 的返回值

        void return_value(T value) { result_ = std::move(value); }



        std::coroutine_handle<> continuation_;

        std::exception_ptr exception_;

        T result_;  ///< co_return 的结果

    };



    Task() = default;

    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}



    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    Task& operator=(Task&& other) noexcept {

        if (this != &other) {

            if (handle_) handle_.destroy();

            handle_ = std::exchange(other.handle_, {});

        }

        return *this;

    }



    ~Task() {

        if (handle_) handle_.destroy();

    }



    Task(const Task&) = delete;

    Task& operator=(const Task&) = delete;



    /// @brief co_await 后通过 await_resume 获取返回值

    auto operator co_await() {

        struct Awaiter {

            bool await_ready() noexcept { return handle_.done(); }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept {

                handle_.promise().continuation_ = cont;

                return handle_;

            }

            T await_resume() {

                if (handle_.promise().exception_) {

                    std::rethrow_exception(handle_.promise().exception_);

                }

                return std::move(handle_.promise().result_);

            }

            std::coroutine_handle<promise_type> handle_;

        };

        return Awaiter{handle_};

    }



    void start() {

        if (handle_) handle_.resume();

    }



    bool done() const { return !handle_ || handle_.done(); }



private:

    std::coroutine_handle<promise_type> handle_;

};



}  // namespace lightnet

