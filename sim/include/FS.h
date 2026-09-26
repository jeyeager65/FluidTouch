// Desktop implementation of the Arduino-ESP32 FS / File API, backed by a
// directory on the host (see SD.h).
#pragma once

#include <Arduino.h>
#include <memory>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {

class FileImpl;

class File : public Stream {
public:
    File() = default;
    explicit File(std::shared_ptr<FileImpl> impl) : impl_(std::move(impl)) {}

    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buf, size_t size) override;
    using Print::write;
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;
    size_t read(uint8_t *buf, size_t size);
    size_t readBytes(char *buffer, size_t length) { return read(reinterpret_cast<uint8_t *>(buffer), length); }
    bool seek(uint32_t pos);
    size_t position() const;
    size_t size() const;
    void close();
    explicit operator bool() const;
    const char *path() const;
    const char *name() const;
    bool isDirectory() const;
    File openNextFile(const char *mode = FILE_READ);
    void rewindDirectory();
    time_t getLastWrite();

private:
    std::shared_ptr<FileImpl> impl_;
};

// Root of a mounted filesystem - maps "/foo" to <root>/foo on the host
class FS {
public:
    explicit FS(const char *hostSubdir) : subdir_(hostSubdir) {}
    File open(const char *path, const char *mode = FILE_READ, bool create = false);
    File open(const String &path, const char *mode = FILE_READ, bool create = false) {
        return open(path.c_str(), mode, create);
    }
    bool exists(const char *path);
    bool exists(const String &path) { return exists(path.c_str()); }
    bool remove(const char *path);
    bool remove(const String &path) { return remove(path.c_str()); }
    bool rename(const char *from, const char *to);
    bool mkdir(const char *path);
    bool mkdir(const String &path) { return mkdir(path.c_str()); }
    bool rmdir(const char *path);
    bool rmdir(const String &path) { return rmdir(path.c_str()); }

    std::string hostPath(const char *path) const;

protected:
    const char *subdir_;
};

} // namespace fs

using fs::File;
using fs::FS;
