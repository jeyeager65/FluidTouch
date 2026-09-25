// Desktop FS/File/SD implementation using std::filesystem.

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

#include <cstdio>
#include <filesystem>
#include <vector>

namespace stdfs = std::filesystem;

SPIClass SPI;
TwoWire Wire;
fs::SDFS SD;

namespace fs {

class FileImpl {
public:
    std::string vpath;  // path as the firmware sees it, e.g. "/gcode/a.nc"
    std::string name;   // base name
    stdfs::path host;
    FILE *fp = nullptr;
    bool dir = false;
    std::vector<stdfs::path> entries;  // directory snapshot for openNextFile
    size_t nextEntry = 0;

    ~FileImpl() { if (fp) fclose(fp); }
};

// --- File -----------------------------------------------------------------
size_t File::write(uint8_t c) { return write(&c, 1); }

size_t File::write(const uint8_t *buf, size_t size) {
    if (!impl_ || !impl_->fp) return 0;
    return fwrite(buf, 1, size, impl_->fp);
}

int File::available() {
    if (!impl_ || !impl_->fp) return 0;
    long pos = ftell(impl_->fp);
    size_t total = size();
    return pos < 0 || (size_t)pos >= total ? 0 : int(total - pos);
}

int File::read() {
    uint8_t c;
    return read(&c, 1) == 1 ? c : -1;
}

size_t File::read(uint8_t *buf, size_t size) {
    if (!impl_ || !impl_->fp) return 0;
    return fread(buf, 1, size, impl_->fp);
}

int File::peek() {
    if (!impl_ || !impl_->fp) return -1;
    int c = fgetc(impl_->fp);
    if (c != EOF) ungetc(c, impl_->fp);
    return c == EOF ? -1 : c;
}

void File::flush() { if (impl_ && impl_->fp) fflush(impl_->fp); }
bool File::seek(uint32_t pos) { return impl_ && impl_->fp && fseek(impl_->fp, pos, SEEK_SET) == 0; }
size_t File::position() const { return impl_ && impl_->fp ? (size_t)ftell(impl_->fp) : 0; }

size_t File::size() const {
    if (!impl_ || impl_->dir) return 0;
    if (impl_->fp) fflush(impl_->fp);
    std::error_code ec;
    auto s = stdfs::file_size(impl_->host, ec);
    return ec ? 0 : (size_t)s;
}

void File::close() { impl_.reset(); }
File::operator bool() const { return (bool)impl_; }
const char *File::path() const { return impl_ ? impl_->vpath.c_str() : nullptr; }
const char *File::name() const { return impl_ ? impl_->name.c_str() : nullptr; }
bool File::isDirectory() const { return impl_ && impl_->dir; }

File File::openNextFile(const char *mode) {
    if (!impl_ || !impl_->dir || impl_->nextEntry >= impl_->entries.size()) return File();
    const stdfs::path &p = impl_->entries[impl_->nextEntry++];
    std::string child = impl_->vpath;
    if (child.empty() || child.back() != '/') child += '/';
    child += p.filename().string();
    return SD.open(child.c_str(), mode);
}

void File::rewindDirectory() { if (impl_) impl_->nextEntry = 0; }

time_t File::getLastWrite() {
    if (!impl_) return 0;
    std::error_code ec;
    auto t = stdfs::last_write_time(impl_->host, ec);
    if (ec) return 0;
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        t - stdfs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(sys);
}

// --- FS -------------------------------------------------------------------
std::string FS::hostPath(const char *path) const {
    std::string rel = path ? path : "";
    while (!rel.empty() && rel.front() == '/') rel.erase(0, 1);
    return (stdfs::path(sim::dataDir()) / subdir_ / stdfs::path(rel)).lexically_normal().string();
}

File FS::open(const char *path, const char *mode, bool create) {
    if (!path) return File();
    auto impl = std::make_shared<FileImpl>();
    impl->vpath = path;
    impl->host = hostPath(path);
    std::string vp = impl->vpath;
    while (vp.size() > 1 && vp.back() == '/') vp.pop_back();
    impl->name = vp == "/" ? "/" : stdfs::path(vp).filename().string();

    std::error_code ec;
    bool reading = mode && mode[0] == 'r';
    if (reading) {
        if (stdfs::is_directory(impl->host, ec)) {
            impl->dir = true;
            for (auto &entry : stdfs::directory_iterator(impl->host, ec)) {
                impl->entries.push_back(entry.path());
            }
            return File(impl);
        }
        if (!stdfs::exists(impl->host, ec)) return File();
    } else {
        stdfs::create_directories(stdfs::path(impl->host).parent_path(), ec);
    }
    const char *fmode = mode[0] == 'w' ? "wb" : mode[0] == 'a' ? "ab" : "rb";
    impl->fp = fopen(impl->host.string().c_str(), fmode);
    if (!impl->fp) return File();
    return File(impl);
}

bool FS::exists(const char *path) {
    std::error_code ec;
    return stdfs::exists(hostPath(path), ec);
}

bool FS::remove(const char *path) {
    std::error_code ec;
    return stdfs::is_regular_file(hostPath(path), ec) && stdfs::remove(hostPath(path), ec);
}

bool FS::rename(const char *from, const char *to) {
    std::error_code ec;
    stdfs::rename(hostPath(from), hostPath(to), ec);
    return !ec;
}

bool FS::mkdir(const char *path) {
    std::error_code ec;
    stdfs::create_directories(hostPath(path), ec);
    return !ec;
}

bool FS::rmdir(const char *path) {
    std::error_code ec;
    return stdfs::is_directory(hostPath(path), ec) && stdfs::remove(hostPath(path), ec);
}

// --- SD -------------------------------------------------------------------
namespace {
bool sdPresent() {
    std::error_code ec;
    return stdfs::is_directory(stdfs::path(sim::dataDir()) / "sd", ec);
}
} // namespace

bool SDFS::begin(uint8_t, SPIClass &, uint32_t, const char *, uint8_t, bool) {
    if (!sdPresent()) {
        printf("[SIM] SD: no card (create %s/sd to insert one)\n", sim::dataDir());
    }
    return sdPresent();
}

sdcard_type_t SDFS::cardType() { return sdPresent() ? CARD_SDHC : CARD_NONE; }
uint64_t SDFS::cardSize() { return sdPresent() ? 32ULL * 1024 * 1024 * 1024 : 0; }
uint64_t SDFS::totalBytes() { return cardSize(); }

uint64_t SDFS::usedBytes() {
    uint64_t total = 0;
    std::error_code ec;
    for (auto &e : stdfs::recursive_directory_iterator(stdfs::path(sim::dataDir()) / "sd", ec)) {
        if (e.is_regular_file(ec)) total += e.file_size(ec);
    }
    return total;
}

} // namespace fs
