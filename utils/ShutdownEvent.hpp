#ifndef  __cpp_speedest_utils_ctrlc_ShutdownEvent_HPP__
# define __cpp_speedest_utils_ctrlc_ShutdownEvent_HPP__

# include <cstddef>

namespace speedtest::utils {
class ShutdownEvent {
protected:
    ShutdownEvent() = default;

    ShutdownEvent(const ShutdownEvent&) = default;
    ShutdownEvent(ShutdownEvent&&) = default;

    ShutdownEvent& operator = (const ShutdownEvent&) = default;
    ShutdownEvent& operator = (ShutdownEvent&&) = default;

    ~ShutdownEvent() = default;

public:
    /**
     * Has shutdown event happens
     */
    virtual bool has_event() const noexcept = 0;
};

class FakeShutdownEvent: public ShutdownEvent {
public:
    FakeShutdownEvent() = default;

    FakeShutdownEvent(const FakeShutdownEvent&) = default;
    FakeShutdownEvent(FakeShutdownEvent&&) = default;

    FakeShutdownEvent& operator = (const FakeShutdownEvent&) = default;
    FakeShutdownEvent& operator = (FakeShutdownEvent&&) = default;

    bool has_event() const noexcept;
};

class CtrlCShutdownEvent: public ShutdownEvent {
    static bool is_triggered;

public:
    /**
     * ctor would call utils::sigaction to register signal handler
     * for SIGINT.
     *
     * User of this class must not set signal handler for SIGINT
     * to something else.
     */
    CtrlCShutdownEvent() noexcept;

    /**
     * This class doesn't have any non-static variable,
     * thus it is safe to copy around once it is 
     * initialized (signal handle is set up).
     */
    CtrlCShutdownEvent(const CtrlCShutdownEvent&) = default;
    /**
     * This class doesn't have any non-static variable,
     * thus it is safe to copy around once it is 
     * initialized (signal handle is set up).
     */
    CtrlCShutdownEvent& operator = (const CtrlCShutdownEvent&) = default;

    /**
     * Destruct object of this class would make no difference.
     *
     * It won't reset signal handler for SIGINT, thus
     * other object of this class is still perfectly usable.
     */
    ~CtrlCShutdownEvent() = default;

    bool has_event() const noexcept;
};
} /* namespace speedtest::utils */

#endif
