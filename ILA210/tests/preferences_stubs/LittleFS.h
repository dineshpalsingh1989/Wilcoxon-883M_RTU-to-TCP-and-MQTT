#pragma once
#include <Arduino.h>
#include <map>
class File {
    std::string path_;
    bool opened_ = false, write_ = false;
public:
    static std::map<std::string, String> storage;
    static bool shortWrite, corruptRead;
    File() {}
    File(const String& path, bool write, bool exists) : path_(path.c_str()), opened_(exists || write), write_(write) {
        if (write) storage[path_] = "";
    }
    explicit operator bool() const { return opened_; }
    size_t size() const { return storage[path_].length(); }
    size_t print(const String& value) {
        if (!opened_ || !write_) return 0;
        storage[path_] = shortWrite ? String("{") : value;
        return storage[path_].length();
    }
    String readString() { return corruptRead ? String("damaged") : storage[path_]; }
    void close() { opened_ = false; }
};
class MockLittleFS {
public:
    static bool failRename;
    bool exists(const String& path) { return File::storage.count(path.c_str()) != 0; }
    bool mkdir(const char*) { return true; }
    File open(const String& path, const char* mode) { return File(path, *mode == 'w', exists(path)); }
    bool rename(const String& from, const String& to) {
        if (failRename || !exists(from)) return false;
        File::storage[to.c_str()] = File::storage[from.c_str()];
        File::storage.erase(from.c_str()); return true;
    }
    bool remove(const String& path) { return File::storage.erase(path.c_str()) != 0; }
};
extern MockLittleFS LittleFS;
