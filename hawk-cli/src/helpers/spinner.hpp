#ifndef HAWK_CLI_SPINNER_HPP
#define HAWK_CLI_SPINNER_HPP

#include <helpers/console.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace hawk::cli {

class Spinner {
public:
    explicit Spinner(std::string_view message)
        : running_(false)
    {
        if (!console::stderr_is_tty()) {
            return;  // no TTY → spinner is a no-op
        }

        running_ = true;
        thread_  = std::thread([this, msg = std::string(message)]() {
            const std::string frames[] = {"⢎ ", "⠎⠁", "⠊⠑", "⠈⠱", " ⡱", "⢀⡰", "⢄⡠", "⢆⡀"};
            int i = 0;
            while (running_) {
                std::cerr << "\r" << frames[i++ % 8] << " " << msg;
                std::cerr.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }
            // Clear the spinner line
            std::cerr << "\r" << std::string(msg.size() + 4, ' ') << "\r";
            std::cerr.flush();
        });
    }

    ~Spinner() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
    }

    // Non-copyable, non-movable
    Spinner(const Spinner&)            = delete;
    Spinner& operator=(const Spinner&) = delete;
    Spinner(Spinner&&)                 = delete;
    Spinner& operator=(Spinner&&)      = delete;

private:
    std::atomic<bool> running_;
    std::thread       thread_;
};

} // namespace hawk::cli

#endif // HAWK_CLI_SPINNER_HPP
