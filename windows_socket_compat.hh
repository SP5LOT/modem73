#pragma once
// =============================================================================
// Windows socket compatibility layer for MinGW/UCRT64
// Zastępuje POSIX <sys/socket.h>, <netinet/in.h>, <arpa/inet.h>, <poll.h>
// =============================================================================
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7+
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

// Winsock2 / winnt.h definiuje makro IN= do SAL annotations - koliduje z
// nazwami template parameters w aicodix (template<typename IN>).
// Musimy je undefined-ować.
#undef IN
#undef OUT
#undef OPTIONAL

// MSG_NOSIGNAL: Windows nie ma SIGPIPE dla socketów
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

// MSG_DONTWAIT: na Windows socket jest nonblocking przez ioctlsocket
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif

// TCP_NODELAY: zazwyczaj w ws2tcpip.h
#ifndef TCP_NODELAY
#define TCP_NODELAY 0x0001
#endif

// poll() przez WSAPoll (dostępny od Vista+)
#ifndef POLLIN
#  define POLLIN   0x0100
#  define POLLOUT  0x0010
#  define POLLERR  0x0001
#  define POLLHUP  0x0002
#  define POLLNVAL 0x0004
   struct pollfd { SOCKET fd; short events; short revents; };
   static inline int poll(struct pollfd* fds, unsigned long nfds, int timeout_ms) {
       return WSAPoll(reinterpret_cast<WSAPOLLFD*>(fds), nfds, timeout_ms);
   }
#endif

// closesocket() zamiast POSIX close() dla socket FDs
#define WIN_CLOSE_SOCKET(s) closesocket(static_cast<SOCKET>(s))

// Auto-inicjalizacja WSA przy starcie programu
namespace _win_socket_compat {
    struct WsaInit {
        WsaInit()  { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
        ~WsaInit() { WSACleanup(); }
    };
    static WsaInit _instance;
}

#endif // _WIN32
