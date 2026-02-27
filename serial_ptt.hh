#pragma once

#include <string>

enum class PTTLine {
    DTR = 0,
    RTS = 1,
    BOTH = 2
};

#ifdef _WIN32

#include <windows.h>
#include <iostream>

class SerialPTT {
public:
    SerialPTT() = default;

    ~SerialPTT() {
        close();
    }

    bool open(const std::string& port, PTTLine line = PTTLine::RTS,
              bool invert_dtr = false, bool invert_rts = false) {
        close();

        port_       = port;
        line_       = line;
        invert_dtr_ = invert_dtr;
        invert_rts_ = invert_rts;

        // Windows wymaga formatu \\.\COMx dla portów >= COM10
        std::string dev = "\\\\.\\" + port;
        // Jeśli user poda /dev/ttyS0 albo już pełną ścieżkę — użyj wprost
        if (port.find('\\') != std::string::npos || port.find('/') != std::string::npos)
            dev = port;

        handle_ = CreateFileA(
            dev.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        if (handle_ == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            last_error_ = "Nie można otworzyć " + port + " (WinAPI błąd " + std::to_string(err) + ")";
            handle_ = nullptr;
            return false;
        }

        ptt_off();
        return true;
    }

    void close() {
        if (handle_) {
            ptt_off();
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

    bool is_open() const { return handle_ != nullptr; }

    const std::string& last_error() const { return last_error_; }

    void ptt_on() {
        if (!handle_) return;
        if (line_ == PTTLine::RTS || line_ == PTTLine::BOTH)
            EscapeCommFunction(handle_, invert_rts_ ? CLRRTS : SETRTS);
        if (line_ == PTTLine::DTR || line_ == PTTLine::BOTH)
            EscapeCommFunction(handle_, invert_dtr_ ? CLRDTR : SETDTR);
    }

    void ptt_off() {
        if (!handle_) return;
        if (line_ == PTTLine::RTS || line_ == PTTLine::BOTH)
            EscapeCommFunction(handle_, invert_rts_ ? SETRTS : CLRRTS);
        if (line_ == PTTLine::DTR || line_ == PTTLine::BOTH)
            EscapeCommFunction(handle_, invert_dtr_ ? SETDTR : CLRDTR);
    }

    bool reconnect() {
        PTTLine saved_line = line_;
        bool saved_idtr = invert_dtr_, saved_irts = invert_rts_;
        std::string saved_port = port_;
        close();
        return open(saved_port, saved_line, saved_idtr, saved_irts);
    }

private:
    HANDLE handle_   = nullptr;
    PTTLine line_    = PTTLine::RTS;
    bool invert_dtr_ = false;
    bool invert_rts_ = false;
    std::string port_;
    std::string last_error_;
};

#else
// ---- POSIX (Linux/macOS) ----
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <cerrno>
#include <cstring>

class SerialPTT {
public:
    SerialPTT() = default;
    ~SerialPTT() { close(); }

    bool open(const std::string& port, PTTLine line = PTTLine::RTS,
              bool invert_dtr = false, bool invert_rts = false) {
        close();
        port_       = port;
        line_       = line;
        invert_dtr_ = invert_dtr;
        invert_rts_ = invert_rts;

        fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fd_ < 0) {
            last_error_ = std::string("Failed to open ") + port + ": " + strerror(errno);
            return false;
        }
        struct termios tty;
        if (tcgetattr(fd_, &tty) == 0) {
            cfmakeraw(&tty);
            tcsetattr(fd_, TCSANOW, &tty);
        }
        ptt_off();
        open_ = true;
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ptt_off();
            ::close(fd_);
            fd_ = -1;
        }
        open_ = false;
    }

    bool is_open() const { return open_; }
    const std::string& last_error() const { return last_error_; }

    void ptt_on() {
        if (fd_ < 0) return;
        int flags = 0;
        ioctl(fd_, TIOCMGET, &flags);
        if (line_ == PTTLine::DTR || line_ == PTTLine::BOTH)
            invert_dtr_ ? (flags &= ~TIOCM_DTR) : (flags |= TIOCM_DTR);
        if (line_ == PTTLine::RTS || line_ == PTTLine::BOTH)
            invert_rts_ ? (flags &= ~TIOCM_RTS) : (flags |= TIOCM_RTS);
        ioctl(fd_, TIOCMSET, &flags);
    }

    void ptt_off() {
        if (fd_ < 0) return;
        int flags = 0;
        ioctl(fd_, TIOCMGET, &flags);
        if (line_ == PTTLine::DTR || line_ == PTTLine::BOTH)
            invert_dtr_ ? (flags |= TIOCM_DTR) : (flags &= ~TIOCM_DTR);
        if (line_ == PTTLine::RTS || line_ == PTTLine::BOTH)
            invert_rts_ ? (flags |= TIOCM_RTS) : (flags &= ~TIOCM_RTS);
        ioctl(fd_, TIOCMSET, &flags);
    }

    bool reconnect() {
        std::string p = port_; PTTLine l = line_;
        bool id = invert_dtr_, ir = invert_rts_;
        close();
        return open(p, l, id, ir);
    }

private:
    int fd_          = -1;
    bool open_       = false;
    PTTLine line_    = PTTLine::RTS;
    bool invert_dtr_ = false;
    bool invert_rts_ = false;
    std::string port_;
    std::string last_error_;
};

#endif // _WIN32
