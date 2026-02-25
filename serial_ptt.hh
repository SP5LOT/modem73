#pragma once
// Serial PTT stub dla Windows (termios/ioctl niedostępne pod MinGW)
// Użyj PTT: none, vox lub rigctl
#include <string>
#include <iostream>

enum class PTTLine { RTS, DTR, BOTH };

class SerialPTT {
public:
    std::string last_error() const {
        return "Serial PTT nie jest wspierany na Windows";
    }
    bool open(const std::string& port, PTTLine /*line*/, bool /*invert_dtr*/ = false, bool /*invert_rts*/ = false) {
        std::cerr << "SerialPTT: niedostepny na Windows. Port: " << port << std::endl;
        return false;
    }
    void ptt_on()  { }
    void ptt_off() { }
    void close()   { }
    bool is_open() const { return false; }
};
