#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#define RAPL_DIR     "/sys/class/powercap/intel-rapl:0/energy_uj"
#define DURATION_US  60000000          // 60 s
#define MAX_SAMPLES  100000000UL       // safety cap on the buffer

static int rapl_fd = -1;
static int stat_fd = -1;

static inline struct timespec get_time_stamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts;
}

typedef struct {
    int64_t time_ns;
    int64_t energy_uj;
} sample_t;

int open_file(void) {
    rapl_fd = open(RAPL_DIR, O_RDONLY);
    if (rapl_fd < 0) {
        perror("open energy_uj (try sudo)");
        return -1;
    }
    return 0;
}

static inline int64_t diff_ns(struct timespec a, struct timespec b) {
    return (int64_t)(a.tv_sec - b.tv_sec) * 1000000000LL
         + (a.tv_nsec - b.tv_nsec);
}

static inline void add_us(struct timespec *ts, int us) {
    ts->tv_nsec += (long)us * 1000;           // µs -> ns
    while (ts->tv_nsec >= 1000000000L) {      // carry into seconds
        ts->tv_nsec -= 1000000000L;
        ts->tv_sec++;
    }
}

int64_t read_cpu_file(void) {
    char buf[32];
    ssize_t n = pread(rapl_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        perror("pread energy_uj");
        return -1;
    }
    buf[n] = '\0';
    return (int64_t)strtoull(buf, NULL, 10);
}

void reader_close(void) {
    if (rapl_fd >= 0) {
        close(rapl_fd);
        rapl_fd = -1;
    }
    if (stat_fd >= 0) {
        close(stat_fd);
        stat_fd = -1;
    }
}

int write_csv(const char *path, const sample_t *samples, size_t count) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        perror("fopen csv");
        return -1;
    }

    fprintf(f, "sample,time_ns,energy_uj\n");

    for (size_t i = 0; i < count; i++) {
        fprintf(f, "%zu,%lld,%lld\n", i + 1,
                (long long)samples[i].time_ns,
                (long long)samples[i].energy_uj);
    }

    fclose(f);
    return 0;
}

int main(void) {
    sample_t *samples = malloc(MAX_SAMPLES * sizeof *samples);
    if (samples == NULL) {
        perror("malloc");
        return 1;
    }

    if (open_file() < 0) {
        free(samples);
        return 1;
    }

    struct timespec start = get_time_stamp();
    struct timespec target = start;
    add_us(&target, DURATION_US);

    struct timespec now;
    size_t i = 0;

    do {
        int64_t e = read_cpu_file();
        now = get_time_stamp();
        if (e < 0) break;                             // read failed, stop

        samples[i].time_ns   = diff_ns(now, start);   // ns since start
        samples[i].energy_uj = e;
        i++;
    } while (i < MAX_SAMPLES && diff_ns(target, now) > 0);

    if (i == MAX_SAMPLES)
        fprintf(stderr, "warning: buffer full, stopped early\n");

    printf("%zu samples in %.3f s\n", i, diff_ns(now, start) / 1e9);

    write_csv("energy.csv", samples, i);
    reader_close();
    free(samples);
    return 0;
}
