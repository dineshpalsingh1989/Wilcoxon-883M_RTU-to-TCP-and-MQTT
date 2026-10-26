#include "WilcoxonMqtt.h"
#include <stdlib.h>
#include <string.h>

namespace {
const uint8_t Connect = 0x10, ConnAck = 0x20, Publish = 0x30, PubAck = 0x40, Subscribe = 0x80,
              Unsubscribe = 0xA0, PingReq = 0xC0, PingResp = 0xD0, Disconnect = 0xE0;
const uint32_t MaxRemaining = 268435455UL; // largest length the 4-byte MQTT encoding holds

size_t putString(uint8_t* out, const char* text, size_t length) {
    out[0] = length >> 8;
    out[1] = length & 0xFF;
    memcpy(out + 2, text, length);
    return length + 2;
}
}

WilcoxonMqttClient::~WilcoxonMqttClient() { free(buffer_); }

bool WilcoxonMqttClient::setBufferSize(uint16_t size) {
    if (size < 16) return false;
    uint8_t* resized = static_cast<uint8_t*>(realloc(buffer_, size));
    if (!resized) return false;
    buffer_ = resized;
    bufferSize_ = size;
    return true;
}

bool WilcoxonMqttClient::ensureBuffer() { return buffer_ || setBufferSize(256); }

// Writes the fixed header at the start of the buffer and returns its length.
size_t WilcoxonMqttClient::putHeader(uint8_t type, uint32_t remaining) {
    size_t n = 0;
    buffer_[n++] = type;
    do {
        uint8_t digit = remaining % 128;
        remaining /= 128;
        if (remaining) digit |= 0x80;
        buffer_[n++] = digit;
    } while (remaining);
    return n;
}

bool WilcoxonMqttClient::writeAll(const uint8_t* data, size_t length) {
    uint32_t progress = millis();
    while (length) {
        const size_t sent = client_->write(data, length);
        if (sent > length) { closeWith(ConnectionLost); return false; }
        if (sent) {
            data += sent;
            length -= sent;
            progress = millis();
        } else if (!client_->connected() || millis() - progress >= socketTimeout_ * 1000UL) {
            closeWith(ConnectionLost); // a partial packet would leave the broker stream out of step
            return false;
        } else {
            delay(1);
        }
    }
    lastOut_ = millis();
    return true;
}

bool WilcoxonMqttClient::sendSimple(uint8_t type, uint16_t id, bool withId) {
    uint8_t packet[4] = {type, uint8_t(withId ? 2 : 0), uint8_t(id >> 8), uint8_t(id & 0xFF)};
    return writeAll(packet, withId ? 4 : 2);
}

bool WilcoxonMqttClient::waitAvailable() {
    const uint32_t start = millis();
    while (client_->available() <= 0) {
        if (!client_->connected() || millis() - start >= socketTimeout_ * 1000UL) return false;
        delay(1);
    }
    return true;
}

bool WilcoxonMqttClient::readByte(uint8_t& value) {
    if (!waitAvailable()) return false;
    const int c = client_->read();
    if (c < 0) return false;
    value = uint8_t(c);
    return true;
}

// Reads one packet into the buffer. Returns its length, 0 when it was larger than the
// buffer (read and discarded), or -1 when the stream broke off mid-packet.
long WilcoxonMqttClient::readPacket() {
    uint8_t byte = 0;
    if (!readByte(byte)) return -1;
    buffer_[0] = byte;
    size_t stored = 1;
    uint32_t remaining = 0, multiplier = 1;
    for (int i = 0;; i++) {
        if (i == 4 || !readByte(byte)) return -1; // more than 4 length bytes is a protocol error
        if (stored < bufferSize_) buffer_[stored] = byte;
        stored++;
        remaining += (byte & 0x7F) * multiplier;
        multiplier *= 128;
        if (!(byte & 0x80)) break;
    }
    const size_t total = stored + remaining;
    for (uint32_t i = 0; i < remaining; i++) {
        if (!readByte(byte)) return -1;
        if (stored < bufferSize_) buffer_[stored] = byte;
        stored++;
    }
    lastIn_ = millis();
    if (total > bufferSize_) { dropped_++; return 0; }
    return long(total);
}

bool WilcoxonMqttClient::connect(const char* id, const char* user, const char* pass) {
    if (connected()) return true;
    if (!client_ || !id || !ensureBuffer()) { state_ = ConnectFailed; return false; }
    const size_t idLength = strlen(id), userLength = user ? strlen(user) : 0, passLength = user && pass ? strlen(pass) : 0;
    const uint32_t remaining = 10 + 2 + idLength + (user ? 2 + userLength : 0) + (user && pass ? 2 + passLength : 0);
    if (MaxHeader + remaining > bufferSize_ || idLength > 0xFFFF || userLength > 0xFFFF || passLength > 0xFFFF) {
        state_ = ConnectFailed;
        return false;
    }
    const int opened = host_ ? client_->connect(host_, port_) : client_->connect(ip_, port_);
    if (opened != 1) { state_ = ConnectFailed; return false; }

    size_t n = putHeader(Connect, remaining);
    const uint8_t variable[] = {0, 4, 'M', 'Q', 'T', 'T', 4};
    memcpy(buffer_ + n, variable, sizeof(variable));
    n += sizeof(variable);
    buffer_[n++] = 0x02 | (user ? 0x80 : 0) | (user && pass ? 0x40 : 0); // clean session
    buffer_[n++] = keepAlive_ >> 8;
    buffer_[n++] = keepAlive_ & 0xFF;
    n += putString(buffer_ + n, id, idLength);
    if (user) n += putString(buffer_ + n, user, userLength);
    if (user && pass) n += putString(buffer_ + n, pass, passLength);
    if (!writeAll(buffer_, n)) { state_ = ConnectFailed; return false; }

    if (!waitAvailable()) { closeWith(ConnectionTimeout); return false; }
    const long length = readPacket();
    if (length < 4 || (buffer_[0] & 0xF0) != ConnAck) { closeWith(ConnectionLost); return false; }
    if (buffer_[3] != 0) { closeWith(buffer_[3]); return false; }
    connected_ = true;
    pingOutstanding_ = false;
    lastIn_ = lastOut_ = millis();
    connects_++;
    state_ = Connected;
    return true;
}

void WilcoxonMqttClient::disconnect() {
    if (connected_ && client_->connected()) sendSimple(Disconnect, 0, false);
    closeWith(Disconnected);
}

void WilcoxonMqttClient::closeWith(int state) {
    connected_ = false;
    pingOutstanding_ = false;
    state_ = state;
    if (client_) client_->stop();
}

bool WilcoxonMqttClient::connected() {
    if (!client_ || !connected_) return false;
    if (!client_->connected()) closeWith(ConnectionLost);
    return connected_;
}

bool WilcoxonMqttClient::loop() {
    if (!connected()) return false;
    const uint32_t now = millis(), keepAlive = keepAlive_ * 1000UL;
    if (keepAlive && (now - lastIn_ >= keepAlive || now - lastOut_ >= keepAlive)) {
        if (pingOutstanding_) { closeWith(ConnectionTimeout); return false; }
        if (!sendSimple(PingReq, 0, false)) return false;
        lastIn_ = now;
        pingOutstanding_ = true;
    }
    for (int handled = 0; handled < 4 && client_->available() > 0; handled++) {
        const long length = readPacket();
        if (length < 0) { closeWith(ConnectionLost); return false; }
        if (length == 0) continue;
        const uint8_t type = buffer_[0] & 0xF0;
        if (type == Publish) handlePublish(size_t(length));
        else if (type == PingReq && !sendSimple(PingResp, 0, false)) return false;
        else if (type == PingResp) pingOutstanding_ = false;
        if (!connected_) return false; // a reply failed or the callback disconnected
    }
    return connected_;
}

void WilcoxonMqttClient::handlePublish(size_t length) {
    size_t header = 1;
    while (header < length && (buffer_[header] & 0x80)) header++;
    header++; // past the last length byte
    if (header + 2 > length) return;
    const size_t topicLength = (size_t(buffer_[header]) << 8) | buffer_[header + 1];
    const uint8_t qos = (buffer_[0] >> 1) & 0x03;
    size_t payload = header + 2 + topicLength;
    if (payload + (qos ? 2 : 0) > length) return;
    const uint16_t id = qos ? (uint16_t(buffer_[payload]) << 8) | buffer_[payload + 1] : 0;
    if (qos) payload += 2;
    // Move the topic one byte down so it can end with '\0'; the payload stays in place.
    memmove(buffer_ + header + 1, buffer_ + header + 2, topicLength);
    buffer_[header + 1 + topicLength] = 0;
    if (callback_) callback_(reinterpret_cast<char*>(buffer_ + header + 1), buffer_ + payload, unsigned(length - payload));
    if (qos == 1 && connected_) sendSimple(PubAck, id, true);
}

bool WilcoxonMqttClient::publish(const char* topic, const char* payload, bool retained) {
    return publish(topic, reinterpret_cast<const uint8_t*>(payload), payload ? unsigned(strlen(payload)) : 0, retained);
}

bool WilcoxonMqttClient::publish(const char* topic, const uint8_t* payload, unsigned int length, bool retained) {
    if (!topic || (!payload && length) || !connected()) return false;
    const size_t topicLength = strlen(topic);
    if (MaxHeader + 2 + topicLength > bufferSize_ || 2 + topicLength + length > MaxRemaining) return false;
    size_t n = putHeader(Publish | (retained ? 1 : 0), 2 + topicLength + length);
    n += putString(buffer_ + n, topic, topicLength);
    return writeAll(buffer_, n) && (!length || writeAll(payload, length));
}

bool WilcoxonMqttClient::subscribe(const char* topic, uint8_t qos) {
    if (!topic || qos > 1 || !connected()) return false;
    const size_t topicLength = strlen(topic);
    if (MaxHeader + 2 + 2 + topicLength + 1 > bufferSize_) return false;
    if (++nextId_ == 0) nextId_ = 1;
    size_t n = putHeader(Subscribe | 0x02, 2 + 2 + topicLength + 1);
    buffer_[n++] = nextId_ >> 8;
    buffer_[n++] = nextId_ & 0xFF;
    n += putString(buffer_ + n, topic, topicLength);
    buffer_[n++] = qos;
    return writeAll(buffer_, n);
}

bool WilcoxonMqttClient::unsubscribe(const char* topic) {
    if (!topic || !connected()) return false;
    const size_t topicLength = strlen(topic);
    if (MaxHeader + 2 + 2 + topicLength > bufferSize_) return false;
    if (++nextId_ == 0) nextId_ = 1;
    size_t n = putHeader(Unsubscribe | 0x02, 2 + 2 + topicLength);
    buffer_[n++] = nextId_ >> 8;
    buffer_[n++] = nextId_ & 0xFF;
    n += putString(buffer_ + n, topic, topicLength);
    return writeAll(buffer_, n);
}
