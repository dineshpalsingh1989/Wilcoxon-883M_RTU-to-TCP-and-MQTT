#include "Wilcoxon883MTCP.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <lwip/sockets.h>
#include <errno.h>
#endif

const uint8_t Wilcoxon883MTCP::MaxClients;

int Wilcoxon883MTCP::Connection::send(const uint8_t* data, size_t size) {
    // Non-blocking so a slow client cannot stall the polling task.
#if defined(ARDUINO_ARCH_RP2040)
    // Write no more than the TCP send buffer holds, so write() returns at once.
    const int room = client.availableForWrite();
    if (room <= 0) return client.connected() ? 0 : -1;
    return int(client.write(data, size < size_t(room) ? size : size_t(room)));
#else
    const int written = ::send(client.fd(), data, size, MSG_DONTWAIT);
    if (written >= 0) return written;
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ? 0 : -1;
#endif
}

bool Wilcoxon883MTCP::begin(uint16_t port, uint8_t maxClients, uint8_t unitId) {
    if (running_ || !port || maxClients < 1 || maxClients > MaxClients || unitId > 247) return false;
    server_.begin(port);
    if (!server_) return false;
    server_.setNoDelay(true);
    maxClients_ = maxClients; unitId_ = unitId; running_ = true;
    return true;
}

void Wilcoxon883MTCP::end() {
    for (auto& session : sessions_) session.close();
    if (running_) server_.end();
    running_ = false;
}

void Wilcoxon883MTCP::poll() {
    if (!running_) return;
    const uint32_t now = millis();
#if defined(ARDUINO_ARCH_RP2040)
    WiFiClient incoming = server_.accept();
#else
    WiFiClient incoming = server_.available();
#endif
    if (incoming) {
        uint8_t slot = 0;
        while (slot < maxClients_ && sessions_[slot].open()) ++slot;
        if (slot == maxClients_) { incoming.stop(); ++stats_.rejected; }
        else { incoming.setNoDelay(true); sessions_[slot].attach(Connection{incoming}, now); }
    }
    for (uint8_t i = 0; i < maxClients_; ++i) sessions_[i].poll(now, bank_, unitId_, timeouts_, stats_);
}

uint8_t Wilcoxon883MTCP::clientCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < maxClients_; ++i) count += sessions_[i].open();
    return count;
}
