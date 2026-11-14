#pragma once
#include "TcpProtocol.h"

namespace wilcoxon883m {

struct TcpStats {
    uint32_t requests = 0;   // Complete requests answered (including exceptions).
    uint32_t exceptions = 0; // Replies carrying a Modbus exception code.
    uint32_t ignored = 0;    // Requests for another unit ID; no reply sent.
    uint32_t malformed = 0;  // Connections closed for an invalid MBAP header.
    uint32_t timeouts = 0;   // Connections closed for idle or incomplete frames.
    uint32_t rejected = 0;   // Connections refused because all client slots were busy.
};
struct TcpTimeouts {
    uint32_t idleMs = 60000; // Close a client with no traffic; 0 disables.
    uint32_t frameMs = 5000; // Close a client that stops part-way through a request.
};

// One client connection with bounded receive/transmit buffers. Connection needs:
// connected(), available(), read() (-1 when empty), send(data, size) returning
// bytes accepted (0 = would block, negative = error), and stop().
template<class Connection> class TcpSession {
    Connection connection_;
    bool open_ = false;
    TcpFramer framer_;
    uint8_t tx_[MaxTcpAdu] = {};
    size_t txSize_ = 0, txSent_ = 0;
    uint32_t lastActivity_ = 0, frameStart_ = 0;
    // Returns false when the connection failed and was closed.
    bool flush(uint32_t now) {
        while (txSent_ < txSize_) {
            const int written = connection_.send(tx_ + txSent_, txSize_ - txSent_);
            if (written < 0) { close(); return false; }
            if (!written) return true;
            txSent_ += size_t(written); lastActivity_ = now;
        }
        txSent_ = txSize_ = 0; return true;
    }
public:
    bool open() const { return open_; }
    void attach(const Connection& connection, uint32_t now) {
        close(); connection_ = connection; open_ = true; lastActivity_ = now;
    }
    void close() {
        if (open_) connection_.stop();
        open_ = false; framer_.reset(); txSize_ = txSent_ = 0;
    }
    // Answers at most one request per call so other cooperative work keeps running.
    void poll(uint32_t now, RegisterBank& bank, uint8_t unitFilter,
              const TcpTimeouts& timeouts, TcpStats& stats) {
        if (!open_) return;
        if (!connection_.connected() && !connection_.available()) { close(); return; }
        if ((timeouts.idleMs && uint32_t(now - lastActivity_) >= timeouts.idleMs) ||
            (framer_.size() && uint32_t(now - frameStart_) >= timeouts.frameMs)) {
            ++stats.timeouts; close(); return;
        }
        // Finish the previous reply before reading the next pipelined request.
        if (txSize_ && (!flush(now) || txSize_)) return;
        for (size_t budget = 0; budget < MaxTcpAdu && connection_.available(); ++budget) {
            const int byte = connection_.read();
            if (byte < 0) break;
            if (!framer_.size()) frameStart_ = now;
            lastActivity_ = now;
            const TcpFramer::State state = framer_.push(uint8_t(byte));
            if (state == TcpFramer::State::Invalid) { ++stats.malformed; close(); return; }
            if (state == TcpFramer::State::Complete) {
                txSize_ = serveTcp(framer_.data(), framer_.size(), unitFilter, tx_, sizeof(tx_), bank);
                framer_.reset();
                if (!txSize_) { ++stats.ignored; return; }
                ++stats.requests;
                if (tx_[7] & 0x80) ++stats.exceptions;
                flush(now); return;
            }
        }
    }
};

} // namespace wilcoxon883m
