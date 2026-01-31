#include "../include/sink.h"

namespace logger {

FileSink::FileSink(const char *path, const char *mode) noexcept
    : file_(path ? std::fopen(path, mode) : nullptr) {}

FileSink::~FileSink() {
    if (file_) {
        std::fclose(file_);
    }
}

void FileSink::Write(const char *data, std::size_t len) {
    if (!file_ || !data || len == 0) {
        return;
    }
    (void)std::fwrite(data, 1, len, file_);
}

void FileSink::Flush() {
    if (file_) {
        std::fflush(file_);
    }
}

ConsoleSink::ConsoleSink(Stream stream) noexcept
    : stream_(stream == Stream::Stdout ? stdout : stderr) {}

void ConsoleSink::Write(const char *data, std::size_t len) {
    if (!data || len == 0) {
        return;
    }
    (void)std::fwrite(data, 1, len, stream_);
}

void ConsoleSink::Flush() {
    (void)std::fflush(stream_);
}

void NullSink::Write(const char *, std::size_t) {}

void NullSink::Flush() {}

} // namespace logger
