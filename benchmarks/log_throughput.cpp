// benchmarks/log_throughput.cpp
//
// Benchmark for low_latency_logger. Measures:
//   1. Producer-side Log() latency (p50 / p99 / p99.9 / max) at a paced rate
//      (no drops, so the numbers describe the hot path, not the drop path)
//   2. Burst throughput (max rate, with drop count)
//   3. Correctness: every accepted record must reach the sink
//   4. The same latency test for two baselines:
//        - synchronous fprintf() to a file
//        - naive async logger (mutex + std::queue<std::string> + condvar)
//
// Build (Release is REQUIRED - the default CMake build has no optimisation):
//   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLLL_BUILD_BENCHMARKS=ON
//   cmake --build build --target log_throughput
//   ./build/log_throughput
//
// Optional args:  ./log_throughput [samples] [paced_rate_msgs_per_sec]
//   defaults:      2000000           500000
//
// If the paced run reports drops > 0, lower the rate (e.g. 200000).

// Keep the one-time stderr drop warning out of the timed path.
#define LOGGER_ENABLE_STDERR_DIAGNOSTICS 0

#include "../include/logger.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#ifndef BENCH_CAPACITY
#define BENCH_CAPACITY (1u << 14) // records; LogRecord is >1 KB, so keep this modest
#endif

using Clock = std::chrono::steady_clock;

static inline std::uint64_t NowNs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}

// Fixed 64-byte message so every logger handles identical payloads.
static const char *kMsg = "benchmark message: order=123456 px=101.25 qty=500 status=FILLED";

// ---------------------------------------------------------------- stats
struct Stats {
    std::vector<std::uint64_t> v;
    void Add(std::uint64_t x) { v.push_back(x); }
    void Sort() { std::sort(v.begin(), v.end()); }
    std::uint64_t Pct(double p) const {
        if (v.empty()) return 0;
        std::size_t i = static_cast<std::size_t>(p / 100.0 * (v.size() - 1));
        return v[i];
    }
    void Print(const char *name) const {
        std::printf("  %-34s n=%-9zu p50=%-6llu p99=%-6llu p99.9=%-7llu max=%-9llu (ns)\n", name, v.size(),
                    (unsigned long long)Pct(50), (unsigned long long)Pct(99), (unsigned long long)Pct(99.9),
                    (unsigned long long)v.back());
    }
};

// Busy-wait until `deadline` (steady-clock ns). Called OUTSIDE the timed region.
static inline void SpinUntil(std::uint64_t deadline) {
    while (NowNs() < deadline) { LOGGER_CPU_RELAX(); }
}

// ---------------------------------------------------------------- counting sink
// Counts records reaching the sink; read only after Logger::Stop() (joined).
class CountingSink final : public logger::Sink {
  public:
    void Write(const char *, std::size_t) override { ++writes_; }
    void Flush() override {}
    std::uint64_t writes() const { return writes_; }
    void reset() { writes_ = 0; }
  private:
    std::uint64_t writes_ = 0;
};

using BenchLogger = logger::Logger<BENCH_CAPACITY>;

// ---------------------------------------------------------------- 0. timer resolution
static void MeasureTimerOverhead() {
    Stats s;
    s.v.reserve(1000000);
    for (int i = 0; i < 1000000; ++i) {
        std::uint64_t a = NowNs();
        std::uint64_t b = NowNs();
        s.Add(b - a);
    }
    s.Sort();
    std::printf("Timer self-cost (back-to-back now() calls, i.e. measurement floor):\n");
    s.Print("empty timed region");
    std::printf("  NOTE: on Apple silicon the clock ticks at 24 MHz, so single-call readings are\n"
                "        quantised to ~41 ns steps. Compare latency to this floor, and rely on the\n"
                "        batch-amortised row for sub-50 ns resolution.\n\n");
}

// ---------------------------------------------------------------- 1. our logger, paced latency
static void LatencyOurLogger(std::size_t samples, std::uint64_t rate) {
    CountingSink sink;
    logger::TextFormatter fmt;
    auto lg = std::make_unique<BenchLogger>(fmt, sink); // heap: ring buffer is huge
    lg->Start();

    const std::uint64_t gap = 1000000000ull / rate;
    Stats per_call, batch;
    per_call.v.reserve(samples);
    constexpr int kBatch = 100;
    batch.v.reserve(samples / kBatch);

    std::uint64_t drops = 0, accepted = 0;

    // warm-up (page-in the ring buffer, warm caches/branch predictors)
    for (std::size_t i = 0; i < 200000; ++i) {
        (void)lg->Log(logger::Level::Info, kMsg);
        std::uint64_t t = NowNs() + gap;
        SpinUntil(t);
    }
    while (lg->PendingCount() != 0) { std::this_thread::yield(); }

    // per-call timing
    std::uint64_t next = NowNs();
    for (std::size_t i = 0; i < samples; ++i) {
        next += gap;
        SpinUntil(next);
        std::uint64_t t0 = NowNs();
        logger::LogResult r = lg->Log(logger::Level::Info, kMsg);
        std::uint64_t t1 = NowNs();
        if (r == logger::LogResult::Success) { per_call.Add(t1 - t0); ++accepted; }
        else { ++drops; }
    }

    // batch-amortised timing (kBatch back-to-back calls per sample)
    next = NowNs();
    for (std::size_t i = 0; i < samples / kBatch; ++i) {
        next += gap * kBatch;
        SpinUntil(next);
        std::uint64_t t0 = NowNs();
        for (int k = 0; k < kBatch; ++k) {
            if (lg->Log(logger::Level::Info, kMsg) != logger::LogResult::Success) ++drops;
        }
        std::uint64_t t1 = NowNs();
        batch.Add((t1 - t0) / kBatch);
    }

    lg->Stop();
    per_call.Sort();
    batch.Sort();

    std::printf("[1] low_latency_logger @ %llu msgs/s paced, capacity=%u records\n", (unsigned long long)rate,
                (unsigned)BENCH_CAPACITY);
    per_call.Print("Log() per call");
    batch.Print("Log() batch-amortised (100/batch)");
    std::printf("  drops during paced run: %llu %s\n\n", (unsigned long long)drops,
                drops ? "<-- NOT a clean run; lower the rate" : "(clean)");
}

// ---------------------------------------------------------------- 2. burst throughput + correctness
static void ThroughputAndCorrectness(std::size_t total) {
    CountingSink sink;
    logger::TextFormatter fmt;
    auto lg = std::make_unique<BenchLogger>(fmt, sink);
    lg->Start();

    std::uint64_t accepted = 0, dropped = 0;
    std::uint64_t t0 = NowNs();
    for (std::size_t i = 0; i < total; ++i) {
        if (lg->Log(logger::Level::Info, kMsg) == logger::LogResult::Success) ++accepted;
        else ++dropped;
    }
    std::uint64_t t_prod = NowNs();
    while (lg->PendingCount() != 0) { std::this_thread::yield(); }
    lg->Stop(); // joins consumer and flushes
    std::uint64_t t_end = NowNs();

    double prod_s = (t_prod - t0) / 1e9, e2e_s = (t_end - t0) / 1e9;
    std::printf("[2] Burst (producer at max speed, %zu attempts)\n", total);
    std::printf("  producer-side attempted rate : %.2f M msgs/s\n", total / prod_s / 1e6);
    std::printf("  accepted / dropped           : %llu / %llu (%.2f%% dropped; drop-on-full is by design)\n",
                (unsigned long long)accepted, (unsigned long long)dropped, 100.0 * dropped / total);
    std::printf("  end-to-end delivered rate    : %.2f M msgs/s (incl. drain)\n", sink.writes() / e2e_s / 1e6);
    std::printf("  CORRECTNESS: accepted=%llu, reached sink=%llu -> %s\n\n", (unsigned long long)accepted,
                (unsigned long long)sink.writes(), accepted == sink.writes() ? "PASS (no loss of accepted records)" : "FAIL");
}

// ---------------------------------------------------------------- 3. baseline: synchronous fprintf
static void BaselineFprintf(std::size_t samples, std::uint64_t rate, const char *path) {
    std::FILE *f = std::fopen(path, "w");
    if (!f) { std::perror("fopen"); return; }
    const std::uint64_t gap = 1000000000ull / rate;
    Stats s;
    s.v.reserve(samples);
    for (std::size_t i = 0; i < 100000; ++i) std::fprintf(f, "[%llu] [INFO] %s\n", (unsigned long long)i, kMsg);
    std::uint64_t next = NowNs();
    for (std::size_t i = 0; i < samples; ++i) {
        next += gap;
        SpinUntil(next);
        std::uint64_t t0 = NowNs();
        std::fprintf(f, "[%llu] [INFO] %s\n", (unsigned long long)i, kMsg);
        std::uint64_t t1 = NowNs();
        s.Add(t1 - t0);
    }
    std::fclose(f);
    std::remove(path);
    s.Sort();
    std::printf("[3] Baseline: synchronous fprintf() to file (stdio-buffered), same rate\n");
    s.Print("fprintf per call");
    std::printf("\n");
}

// ---------------------------------------------------------------- 4. baseline: mutex + queue<string>
class NaiveAsyncLogger {
  public:
    NaiveAsyncLogger() : th_(&NaiveAsyncLogger::Run, this) {}
    ~NaiveAsyncLogger() {
        { std::lock_guard<std::mutex> g(m_); stop_ = true; }
        cv_.notify_one();
        th_.join();
    }
    void Log(const char *msg) {
        {
            std::lock_guard<std::mutex> g(m_);
            q_.emplace(msg); // heap allocation + lock on the hot path
        }
        cv_.notify_one();
    }
  private:
    void Run() {
        std::unique_lock<std::mutex> lk(m_);
        while (true) {
            cv_.wait(lk, [&] { return stop_ || !q_.empty(); });
            while (!q_.empty()) {
                std::string s = std::move(q_.front());
                q_.pop();
                lk.unlock();
                consumed_ += s.size(); // stand-in for formatting + sink
                lk.lock();
            }
            if (stop_) return;
        }
    }
    std::mutex m_;
    std::condition_variable cv_;
    std::queue<std::string> q_;
    bool stop_ = false;
    std::uint64_t consumed_ = 0;
    std::thread th_;
};

static void BaselineMutexQueue(std::size_t samples, std::uint64_t rate) {
    const std::uint64_t gap = 1000000000ull / rate;
    Stats s;
    s.v.reserve(samples);
    {
        NaiveAsyncLogger lg;
        for (std::size_t i = 0; i < 100000; ++i) { lg.Log(kMsg); SpinUntil(NowNs() + gap); }
        std::uint64_t next = NowNs();
        for (std::size_t i = 0; i < samples; ++i) {
            next += gap;
            SpinUntil(next);
            std::uint64_t t0 = NowNs();
            lg.Log(kMsg);
            std::uint64_t t1 = NowNs();
            s.Add(t1 - t0);
        }
    }
    s.Sort();
    std::printf("[4] Baseline: naive async logger (mutex + std::queue<std::string> + condvar), same rate\n");
    s.Print("mutex+queue per call");
    std::printf("\n");
}

// ---------------------------------------------------------------- main
int main(int argc, char **argv) {
    std::size_t samples = argc > 1 ? static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10)) : 2000000;
    std::uint64_t rate = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 500000;
    if (samples < 1000) samples = 1000;
    if (rate < 1000) rate = 1000;

#ifndef __OPTIMIZE__
    std::printf("WARNING: compiled without optimisation. Rebuild with -DCMAKE_BUILD_TYPE=Release.\n"
                "         Numbers below are NOT valid for reporting.\n\n");
#endif
    std::printf("=== low_latency_logger benchmark ===\n");
    std::printf("sizeof(LogRecord)=%zu bytes, kCacheLineSize=%zu, ring capacity=%u records (%.1f MiB)\n",
                sizeof(logger::LogRecord), (std::size_t)logger::internal::kCacheLineSize, (unsigned)BENCH_CAPACITY,
                sizeof(logger::LogRecord) * (double)BENCH_CAPACITY / (1024.0 * 1024.0));
    std::printf("samples=%zu, paced rate=%llu msgs/s, hw threads=%u\n\n", samples, (unsigned long long)rate,
                std::thread::hardware_concurrency());

    MeasureTimerOverhead();
    LatencyOurLogger(samples, rate);
    ThroughputAndCorrectness(5000000);
    BaselineFprintf(samples, rate, "/tmp/lll_bench_sync.log");
    BaselineMutexQueue(samples, rate);

    std::printf("Record these alongside the numbers: chip/OS, compiler+version, flags (Release/-O3), date.\n");
    return 0;
}
