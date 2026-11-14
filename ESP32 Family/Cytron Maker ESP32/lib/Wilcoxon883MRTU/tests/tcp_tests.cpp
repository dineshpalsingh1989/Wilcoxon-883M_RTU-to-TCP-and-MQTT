#include "detail/TcpSession.h"
#include "detail/RegisterBlocks.h"
#include <assert.h>
#include <stdio.h>
using namespace wilcoxon883m;

static size_t request(uint8_t* p, uint16_t transaction, uint8_t unit, uint8_t fn, uint16_t a, uint16_t v) {
    put(p, transaction); put(p + 2, 0); put(p + 4, 6); p[6] = unit;
    p[7] = fn; put(p + 8, a); put(p + 10, v); return 12;
}
static size_t writeMany(uint8_t* p, uint16_t a, uint16_t n, uint16_t first) {
    put(p, 7); put(p + 2, 0); put(p + 4, uint16_t(7 + 2 * n)); p[6] = 1;
    p[7] = 16; put(p + 8, a); put(p + 10, n); p[12] = uint8_t(2 * n);
    for (uint16_t i = 0; i < n; ++i) put(p + 13 + 2 * i, uint16_t(first + i));
    return 13 + 2 * size_t(n);
}

struct Lock { bool held = false, fail = false; unsigned locks = 0; };
static bool lockFn(void* c) { Lock* l = static_cast<Lock*>(c); if (l->fail) return false; assert(!l->held); l->held = true; ++l->locks; return true; }
static void unlockFn(void* c) { Lock* l = static_cast<Lock*>(c); assert(l->held); l->held = false; }
struct WriteLog { Lock* lock; uint16_t address = 0, count = 0; unsigned calls = 0; };
static void writtenFn(uint16_t a, uint16_t n, void* c) {
    WriteLog* w = static_cast<WriteLog*>(c); assert(!w->lock->held);
    w->address = a; w->count = n; ++w->calls;
}

struct Peer { // Shared state behind a fake connection, as a socket would be.
    uint8_t in[2048] = {}, out[2048] = {};
    size_t inSize = 0, inRead = 0, outSize = 0, sendLimit = 1000;
    bool connected = true, stopped = false, sendError = false;
    void feed(const uint8_t* bytes, size_t n) { memcpy(in + inSize, bytes, n); inSize += n; }
};
struct FakeConnection {
    Peer* peer = nullptr;
    bool connected() { return peer->connected; }
    int available() { return int(peer->inSize - peer->inRead); }
    int read() { return peer->inRead < peer->inSize ? peer->in[peer->inRead++] : -1; }
    int send(const uint8_t* data, size_t size) {
        if (peer->sendError) return -1;
        size_t n = size < peer->sendLimit ? size : peer->sendLimit;
        memcpy(peer->out + peer->outSize, data, n); peer->outSize += n; return int(n);
    }
    void stop() { peer->stopped = true; }
};

int main() {
    uint16_t live[100] = {}, wave[200] = {}, inputs[10] = {}, control[4] = {};
    int16_t signedValues[3] = {-1, -32768, 32767};
    for (uint16_t i = 0; i < 100; ++i) live[i] = uint16_t(i * 3);
    for (uint16_t i = 0; i < 200; ++i) wave[i] = uint16_t(0x8000 + i);
    for (uint16_t i = 0; i < 10; ++i) inputs[i] = uint16_t(1000 + i);
    RegisterBlocks<6> bank; Lock lock; WriteLog log; log.lock = &lock;
    assert(bank.addHolding(0, live, 100));
    assert(bank.addHolding(1000, wave, 200));
    assert(bank.addInput(0, inputs, 10));       // Separate address space from holding.
    assert(bank.addHolding(92, control, 4, true) == false); // Overlaps registers 0-99.
    assert(bank.addHolding(300, control, 4, true));
    assert(bank.addHolding(400, signedValues, 3));
    assert(!bank.addHolding(500, static_cast<uint16_t*>(nullptr), 1));
    assert(!bank.addHolding(65535, control, 2));
    assert(!bank.addHolding(600, control, 0));
    bank.setLock(lockFn, unlockFn, &lock); bank.onWrite(writtenFn, &log);
    puts("PASS register block mapping, overlap and bounds rejection");

    uint8_t req[300], out[MaxTcpAdu];
    size_t n = request(req, 0x1234, 1, 3, 1000, 125);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 9 + 250);
    assert(word(out) == 0x1234 && word(out + 2) == 0 && word(out + 4) == 253 && out[6] == 1);
    assert(out[7] == 3 && out[8] == 250);
    for (unsigned i = 0; i < 125; ++i) assert(word(out + 9 + 2 * i) == 0x8000 + i);
    n = request(req, 1, 1, 4, 2, 3);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 15 && word(out + 9) == 1002 && word(out + 13) == 1004);
    n = request(req, 1, 1, 3, 400, 3);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 15 && word(out + 9) == 0xffff && word(out + 11) == 0x8000);
    assert(lock.locks == 3 && !lock.held);
    puts("PASS 125-register read, input/holding spaces, signed arrays, MBAP echo and locking");

    struct Case { uint8_t fn; uint16_t a, v; uint8_t code; } cases[] = {
        {3, 0, 0, IllegalValue}, {3, 0, 126, IllegalValue}, {3, 65535, 2, IllegalAddress},
        {3, 95, 10, IllegalAddress},   // Crosses the end of a block.
        {3, 100, 1, IllegalAddress},   // Unmapped gap.
        {4, 5, 10, IllegalAddress}, {5, 0, 0xff00, IllegalFunction}, {0x2b, 0, 0, IllegalFunction},
        {6, 10, 1, IllegalAddress},    // Read-only block.
    };
    for (const Case& c : cases) {
        n = request(req, 9, 1, c.fn, c.a, c.v);
        assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 9);
        assert(out[7] == (c.fn | 0x80) && out[8] == c.code && word(out + 4) == 3);
    }
    assert(live[10] == 30);
    lock.fail = true; n = request(req, 9, 1, 3, 0, 1);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 9 && out[8] == DeviceBusy);
    lock.fail = false;
    puts("PASS illegal value/address/function exceptions, read-only rejection and busy lock");

    n = request(req, 2, 1, 6, 301, 0xbeef);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 12 && !memcmp(out, req, 12));
    assert(control[1] == 0xbeef && log.calls == 1 && log.address == 301 && log.count == 1);
    n = writeMany(req, 300, 4, 0x10);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 12 && word(out + 10) == 4);
    assert(control[0] == 0x10 && control[3] == 0x13 && log.count == 4);
    n = writeMany(req, 302, 4, 0x20);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 9 && out[8] == IllegalAddress && control[2] == 0x12);
    n = writeMany(req, 300, 2, 0); req[12] = 3;
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 9 && out[8] == IllegalValue);
    n = writeMany(req, 300, 2, 0);
    assert(serveTcp(req, n - 1, 0, out, sizeof(out), bank) == 0); // MBAP length disagrees.
    struct AnyBank : RegisterBank {} empty;
    n = request(req, 3, 1, 6, 0, 1);
    assert(serveTcp(req, n, 0, out, sizeof(out), empty) == 9 && out[8] == IllegalAddress);
    puts("PASS FC06/FC16 writes, echo, partial-write rejection, byte-count and default bank");

    n = request(req, 4, 7, 3, 0, 1);
    assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 11);
    assert(serveTcp(req, n, 7, out, sizeof(out), bank) == 11);
    assert(serveTcp(req, n, 1, out, sizeof(out), bank) == 0);
    req[2] = 1; assert(serveTcp(req, n, 0, out, sizeof(out), bank) == 0);
    puts("PASS unit filter (0 = any) and protocol ID rejection");

    TcpFramer framer; n = request(req, 5, 1, 3, 0, 1);
    for (size_t i = 0; i + 1 < n; ++i) assert(framer.push(req[i]) == TcpFramer::State::More);
    assert(framer.push(req[n - 1]) == TcpFramer::State::Complete && framer.size() == n);
    assert(framer.push(0) == TcpFramer::State::Invalid);
    framer.reset(); put(req + 4, 1);
    for (size_t i = 0; i < 5; ++i) framer.push(req[i]);
    assert(framer.push(req[5]) == TcpFramer::State::Invalid);
    framer.reset(); put(req + 4, 255);
    for (size_t i = 0; i < 5; ++i) framer.push(req[i]);
    assert(framer.push(req[5]) == TcpFramer::State::Invalid);
    puts("PASS framer byte-by-byte assembly and invalid MBAP lengths");

    TcpTimeouts timeouts; TcpStats stats; Peer peer; FakeConnection connection; connection.peer = &peer;
    TcpSession<FakeConnection> session; session.attach(connection, 0);
    size_t first = request(req, 0x0a, 1, 3, 0, 2), second = request(req + first, 0x0b, 1, 3, 2, 1);
    peer.feed(req, first + second); peer.sendLimit = 3; // Pipelined requests, slow socket.
    for (uint32_t t = 1; t < 40 && peer.outSize < 13 + 11; ++t) session.poll(t, bank, 0, timeouts, stats);
    assert(peer.outSize == 24 && word(peer.out) == 0x0a && word(peer.out + 9) == 0 && word(peer.out + 11) == 3);
    assert(word(peer.out + 13) == 0x0b && word(peer.out + 22) == 6 && stats.requests == 2 && session.open());
    peer.sendLimit = 0; n = request(req, 0x0c, 1, 3, 0, 1); peer.feed(req, n);
    session.poll(50, bank, 0, timeouts, stats); session.poll(51, bank, 0, timeouts, stats);
    assert(peer.inRead == peer.inSize && peer.outSize == 24 && session.open()); // Would-block keeps reply.
    peer.sendLimit = 1000; session.poll(52, bank, 0, timeouts, stats);
    assert(peer.outSize == 24 + 11 && word(peer.out + 24) == 0x0c);
    n = request(req, 0x0d, 9, 3, 0, 1); peer.feed(req, n);
    session.poll(53, bank, 1, timeouts, stats);
    assert(stats.ignored == 1 && peer.outSize == 35 && session.open());
    puts("PASS pipelined requests, partial and would-block sends, ignored unit keeps connection");

    peer.feed(req, 5); session.poll(100, bank, 0, timeouts, stats);
    session.poll(100 + timeouts.frameMs - 1, bank, 0, timeouts, stats); assert(session.open());
    session.poll(100 + timeouts.frameMs, bank, 0, timeouts, stats);
    assert(!session.open() && peer.stopped && stats.timeouts == 1);
    Peer idlePeer; FakeConnection idle; idle.peer = &idlePeer; session.attach(idle, 0xfffff000UL);
    session.poll(0xfffff000UL + timeouts.idleMs - 1, bank, 0, timeouts, stats); assert(session.open());
    session.poll(0xfffff000UL + timeouts.idleMs, bank, 0, timeouts, stats); // Wraps millis().
    assert(!session.open() && stats.timeouts == 2);
    Peer badPeer; FakeConnection bad; bad.peer = &badPeer; session.attach(bad, 0);
    n = request(req, 1, 1, 3, 0, 1); req[2] = 1; badPeer.feed(req, n);
    session.poll(1, bank, 0, timeouts, stats); assert(!session.open() && stats.malformed == 1);
    Peer gonePeer; FakeConnection gone; gone.peer = &gonePeer; session.attach(gone, 0);
    gonePeer.connected = false; session.poll(1, bank, 0, timeouts, stats); assert(!session.open());
    Peer errorPeer; FakeConnection error; error.peer = &errorPeer; session.attach(error, 0);
    errorPeer.sendError = true; n = request(req, 1, 1, 3, 0, 1); errorPeer.feed(req, n);
    session.poll(1, bank, 0, timeouts, stats); assert(!session.open() && errorPeer.stopped);
    puts("PASS frame/idle timeouts with millis wrap, malformed header, disconnect and send error");
    puts("All TCP beta library tests passed (simulated sockets; no network hardware).");
}
