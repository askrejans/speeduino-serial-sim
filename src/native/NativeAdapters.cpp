/**
 * @file NativeAdapters.cpp
 * @brief POSIX TCP server and pseudo-terminal for the desktop simulator.
 */
#if defined(NATIVE_SIM) && !defined(ARDUINO)

#include "NativeAdapters.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
  #include <util.h>
#else
  #include <pty.h>
#endif

namespace {
void nonBlocking(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK); }
int pending(int fd) {
    int n = 0;
    return ioctl(fd, FIONREAD, &n) == 0 ? n : 0;
}
}  // namespace

// ---------------------------------------------------------------- TCP

TcpSerial::TcpSerial(const char* bind, uint16_t port) {
    server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return;
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, bind, &addr.sin_addr) != 1 || ::bind(server, (sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(server, 1) != 0) {
        fprintf(stderr, "cannot listen on %s:%u: %s\n", bind, port, strerror(errno));
        close(server);
        server = -1;
        return;
    }
    nonBlocking(server);
}

TcpSerial::~TcpSerial() {
    drop();
    if (server >= 0) close(server);
}

void TcpSerial::accept() {
    if (client >= 0 || server < 0) return;
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    int fd = ::accept(server, (sockaddr*)&peer, &len);
    if (fd < 0) return;
    int yes = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
#endif
    nonBlocking(fd);
    client = fd;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    fprintf(stderr, "client connected from %s\n", ip);
}

void TcpSerial::drop() {
    if (client < 0) return;
    close(client);
    client = -1;
    fprintf(stderr, "client disconnected\n");
}

bool TcpSerial::isReady() {
    accept();
    return client >= 0;
}

int TcpSerial::available() {
    accept();
    if (client < 0) return 0;
    int n = pending(client);
    if (n == 0) {
        // Detect an orderly close: readable with nothing pending.
        char probe;
        ssize_t r = recv(client, &probe, 1, MSG_PEEK);
        if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) drop();
    }
    return n;
}

int TcpSerial::read() {
    if (client < 0) return -1;
    uint8_t b;
    ssize_t r = recv(client, &b, 1, 0);
    if (r == 1) return b;
    if (r == 0) drop();
    return -1;
}

size_t TcpSerial::readBytes(uint8_t* b, size_t n) {
    if (client < 0) return 0;
    ssize_t r = recv(client, b, n, 0);
    return r > 0 ? (size_t)r : 0;
}

size_t TcpSerial::write(const uint8_t* b, size_t n) {
    if (client < 0) return 0;
    size_t sent = 0;
    while (sent < n) {
#ifdef MSG_NOSIGNAL
        ssize_t r = send(client, b + sent, n - sent, MSG_NOSIGNAL);
#else
        ssize_t r = send(client, b + sent, n - sent, 0);
#endif
        if (r > 0) {
            sent += (size_t)r;
        } else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            usleep(200);
        } else {
            drop();
            break;
        }
    }
    return sent;
}

void TcpSerial::clear() {
    uint8_t junk[64];
    while (client >= 0 && pending(client) > 0) readBytes(junk, sizeof(junk));
}

// ---------------------------------------------------------------- PTY

PtySerial::PtySerial() {
    if (openpty(&master, &slave, name, nullptr, nullptr) != 0) {
        master = slave = -1;
        return;
    }
    termios t{};
    tcgetattr(slave, &t);
    cfmakeraw(&t);
    cfsetspeed(&t, B115200);
    tcsetattr(slave, TCSANOW, &t);
    nonBlocking(master);
}

PtySerial::~PtySerial() {
    if (master >= 0) close(master);
    if (slave >= 0) close(slave);
}

int PtySerial::available() { return master >= 0 ? pending(master) : 0; }

int PtySerial::read() {
    uint8_t b;
    return master >= 0 && ::read(master, &b, 1) == 1 ? b : -1;
}

size_t PtySerial::readBytes(uint8_t* b, size_t n) {
    if (master < 0) return 0;
    ssize_t r = ::read(master, b, n);
    return r > 0 ? (size_t)r : 0;
}

size_t PtySerial::write(const uint8_t* b, size_t n) {
    if (master < 0) return 0;
    size_t sent = 0;
    for (int tries = 0; sent < n && tries < 2000; tries++) {
        ssize_t r = ::write(master, b + sent, n - sent);
        if (r > 0) sent += (size_t)r;
        else usleep(100);
    }
    return sent;
}

void PtySerial::clear() {
    uint8_t junk[64];
    while (available() > 0) readBytes(junk, sizeof(junk));
}

#endif
