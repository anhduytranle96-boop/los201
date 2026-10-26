#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEVICE "/dev/sms_sensor"
#define LOGFILE "/tmp/sms.log"
#define BUF_SIZE 64
#define WINDOW 10
#define QUEUE_NAME_ALARM "/sms_alarm_q"
#define QUEUE_NAME_LOG "/sms_log_q"
#define SMS_IOC_MAGIC 'S'
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, uint32_t)

typedef struct {
    long long timestamp;
    double temperature, humidity;
} SensorData;

typedef struct {
    char text[256];
} Message;

typedef struct {
    SensorData items[BUF_SIZE];
    size_t head, tail, size;
    unsigned long produced, consumed, overflow;
    int stopping;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty, not_full;
} CircularBuffer;

static CircularBuffer *buffer;
static mqd_t alarm_q = (mqd_t)-1, log_q = (mqd_t)-1;
static pthread_t sensor_tid, processor_tid, alert_tid, logger_tid;
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t interrupted = 0;
static int device_fd = -1, verbose = 0;
static unsigned int interval_ms = 500, duration_sec = 60;
static double temp_threshold = 40.0, humidity_threshold = 80.0;
static unsigned long alerts_temp = 0, alerts_hum = 0, alerts_spike = 0;

static void on_signal(int sig)
{
    (void)sig;
    interrupted = 1;
}

static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void queue_log(const char *fmt, ...)
{
    Message m;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(m.text, sizeof(m.text), fmt, ap);
    va_end(ap);

    if (log_q != (mqd_t)-1 && mq_send(log_q, (char *)&m, sizeof(m), 0) == -1) {
        if (errno != EAGAIN && errno != EINTR)
            fprintf(stderr, "log queue send: %s\n", strerror(errno));
    }
}

static int buffer_push(SensorData d)
{
    pthread_mutex_lock(&buffer->mutex);
    if (buffer->size == BUF_SIZE) {
        buffer->overflow++;
        pthread_mutex_unlock(&buffer->mutex);
        queue_log("WARN circular buffer full; sample dropped");
        return -1;
    }
    buffer->items[buffer->head] = d;
    buffer->head = (buffer->head + 1) % BUF_SIZE;
    buffer->size++;
    buffer->produced++;
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

static int buffer_pop(SensorData *d)
{
    pthread_mutex_lock(&buffer->mutex);
    while (buffer->size == 0 && !buffer->stopping)
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);

    if (buffer->size == 0 && buffer->stopping) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    *d = buffer->items[buffer->tail];
    buffer->tail = (buffer->tail + 1) % BUF_SIZE;
    buffer->size--;
    buffer->consumed++;
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

static void send_alert(const char *type, double value, double threshold)
{
    Message m;
    snprintf(m.text, sizeof(m.text),
             "[ALERT][%lld] TYPE: %s VALUE: %.3f THRESHOLD: %.3f",
             now_ms(), type, value, threshold);

    if (mq_send(alarm_q, (char *)&m, sizeof(m), 0) == -1)
        fprintf(stderr, "alarm queue send: %s\n", strerror(errno));
}

static void *sensor_thread(void *arg)
{
    (void)arg;
    char line[256];

    while (!interrupted) {
        ssize_t n = read(device_fd, line, sizeof(line) - 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) {
                struct timespec pause = {0, 100000000};
                nanosleep(&pause, NULL);
                continue;
            }
            queue_log("ERROR sensor read: %s", strerror(errno));
            interrupted = 1;
            break;
        }
        if (n == 0) {
            queue_log("WARN sensor returned EOF");
            struct timespec pause = {0, 100000000};
            nanosleep(&pause, NULL);
            continue;
        }

        line[n] = '\0';
        SensorData d;
        if (sscanf(line, "%lld,%lf,%lf",
                   &d.timestamp, &d.temperature, &d.humidity) != 3) {
            queue_log("WARN invalid sensor sample: %s", line);
            continue;
        }

        if (buffer_push(d) == 0 && verbose)
            printf("[SAMPLE] %lld,%.3f,%.3f\n",
                   d.timestamp, d.temperature, d.humidity);

        struct timespec pause = {
            .tv_sec = interval_ms / 1000,
            .tv_nsec = (long)(interval_ms % 1000) * 1000000L
        };
        while (nanosleep(&pause, &pause) == -1 && errno == EINTR && !interrupted)
            ;
    }
    return NULL;
}

static void *processor_thread(void *arg)
{
    (void)arg;
    double temps[WINDOW], hums[WINDOW];
    int count = 0, index = 0;
    double previous_temp = 0;
    int have_previous = 0;

    SensorData d;
    while (buffer_pop(&d) == 0) {
        temps[index] = d.temperature;
        hums[index] = d.humidity;
        index = (index + 1) % WINDOW;
        if (count < WINDOW) count++;

        double tsum = 0, hsum = 0;
        for (int i = 0; i < count; i++) {
            tsum += temps[i];
            hsum += hums[i];
        }

        queue_log("SAMPLE ts=%lld temp=%.3f hum=%.3f avg_temp=%.3f avg_hum=%.3f",
                  d.timestamp, d.temperature, d.humidity,
                  tsum / count, hsum / count);

        if (d.temperature > temp_threshold)
            send_alert("TEMP_HIGH", d.temperature, temp_threshold);
        if (d.humidity > humidity_threshold)
            send_alert("HUMID_HIGH", d.humidity, humidity_threshold);
        if (have_previous && (d.temperature - previous_temp > 5.0 ||
                              previous_temp - d.temperature > 5.0))
            send_alert("TEMP_SPIKE", d.temperature, 5.0);

        previous_temp = d.temperature;
        have_previous = 1;
    }
    return NULL;
}

static void *alert_thread(void *arg)
{
    (void)arg;
    Message m;
    while (!interrupted) {
        ssize_t n = mq_receive(alarm_q, (char *)&m, sizeof(m), NULL);
        if (n >= 0) {
            m.text[sizeof(m.text) - 1] = '\0';
            fprintf(stderr, "%s\n", m.text);
            pthread_mutex_lock(&state_mutex);
            if (strstr(m.text, "TEMP_HIGH")) alerts_temp++;
            else if (strstr(m.text, "HUMID_HIGH")) alerts_hum++;
            else if (strstr(m.text, "TEMP_SPIKE")) alerts_spike++;
            pthread_mutex_unlock(&state_mutex);
            queue_log("%s", m.text);
        } else if (errno != EINTR) {
            break;
        }
    }
    return NULL;
}

static void *logger_thread(void *arg)
{
    (void)arg;
    FILE *fp = fopen(LOGFILE, "a");
    if (!fp) {
        fprintf(stderr, "Cannot open %s: %s\n", LOGFILE, strerror(errno));
        interrupted = 1;
        return NULL;
    }

    time_t last_flush = time(NULL);
    Message m;
    while (!interrupted || (log_q != (mqd_t)-1 &&
                            mq_getattr(log_q, &(struct mq_attr){0}) == 0)) {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 1;

        ssize_t n = mq_timedreceive(log_q, (char *)&m, sizeof(m), NULL, &deadline);
        if (n >= 0) {
            struct stat st;
            if (stat(LOGFILE, &st) == 0 && st.st_size > 1024 * 1024) {
                fclose(fp);
                unlink(LOGFILE ".1");
                if (rename(LOGFILE, LOGFILE ".1") == -1)
                    fprintf(stderr, "log rotate: %s\n", strerror(errno));
                fp = fopen(LOGFILE, "a");
                if (!fp) {
                    interrupted = 1;
                    break;
                }
            }

            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            char stamp[32];
            strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_now);
            fprintf(fp, "[%s] %s\n", stamp, m.text);
        } else if (errno != ETIMEDOUT && errno != EINTR) {
            break;
        }

        if (time(NULL) - last_flush >= 5) {
            fflush(fp);
            last_flush = time(NULL);
        }

        if (interrupted) {
            struct mq_attr attr;
            if (mq_getattr(log_q, &attr) == 0 && attr.mq_curmsgs == 0)
                break;
        }
    }
    fflush(fp);
    fclose(fp);
    return NULL;
}

static int open_queues(void)
{
    struct mq_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(Message);

    mq_unlink(QUEUE_NAME_ALARM);
    mq_unlink(QUEUE_NAME_LOG);
    alarm_q = mq_open(QUEUE_NAME_ALARM, O_CREAT | O_RDWR, 0600, &attr);
    log_q = mq_open(QUEUE_NAME_LOG, O_CREAT | O_RDWR, 0600, &attr);
    return alarm_q == (mqd_t)-1 || log_q == (mqd_t)-1 ? -1 : 0;
}

static void cleanup(void)
{
    if (buffer) {
        pthread_mutex_lock(&buffer->mutex);
        buffer->stopping = 1;
        pthread_cond_broadcast(&buffer->not_empty);
        pthread_mutex_unlock(&buffer->mutex);
    }

    if (device_fd >= 0) close(device_fd);

    if (alarm_q != (mqd_t)-1) mq_close(alarm_q);
    if (log_q != (mqd_t)-1) mq_close(log_q);
    mq_unlink(QUEUE_NAME_ALARM);
    mq_unlink(QUEUE_NAME_LOG);

    if (buffer) {
        pthread_mutex_destroy(&buffer->mutex);
        pthread_cond_destroy(&buffer->not_empty);
        pthread_cond_destroy(&buffer->not_full);
        free(buffer);
        buffer = NULL;
    }
}

static void usage(const char *name)
{
    fprintf(stderr,
        "Usage: %s [-i ms] [-t temperature] [-h humidity] [-d sec] [-v]\n",
        name);
}

int main(int argc, char **argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "i:t:h:d:v")) != -1) {
        char *end = NULL;
        switch (opt) {
        case 'i': {
            unsigned long v = strtoul(optarg, &end, 10);
            if (!end || *end || v < 1 || v > 60000) goto bad_args;
            interval_ms = (unsigned int)v;
            break;
        }
        case 't':
            temp_threshold = strtod(optarg, &end);
            if (!end || *end || temp_threshold < 15 || temp_threshold > 45)
                goto bad_args;
            break;
        case 'h':
            humidity_threshold = strtod(optarg, &end);
            if (!end || *end || humidity_threshold < 30 ||
                humidity_threshold > 90) goto bad_args;
            break;
        case 'd': {
            unsigned long v = strtoul(optarg, &end, 10);
            if (!end || *end || v > 86400) goto bad_args;
            duration_sec = (unsigned int)v;
            break;
        }
        case 'v': verbose = 1; break;
        default: goto bad_args;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    buffer = calloc(1, sizeof(*buffer));
    if (!buffer) {
        perror("calloc");
        return 1;
    }
    pthread_mutex_init(&buffer->mutex, NULL);
    pthread_cond_init(&buffer->not_empty, NULL);
    pthread_cond_init(&buffer->not_full, NULL);

    if (open_queues() == -1) {
        perror("mq_open");
        cleanup();
        return 1;
    }

    device_fd = open(DEVICE, O_RDONLY | O_CLOEXEC);
    if (device_fd == -1) {
        fprintf(stderr, "Cannot open %s: %s\n", DEVICE, strerror(errno));
        cleanup();
        return 1;
    }

    uint32_t interval = interval_ms;
    if (ioctl(device_fd, SMS_SET_INTERVAL, &interval) == -1) {
        perror("ioctl SMS_SET_INTERVAL");
        cleanup();
        return 1;
    }

    if (pthread_create(&logger_tid, NULL, logger_thread, NULL) != 0 ||
        pthread_create(&alert_tid, NULL, alert_thread, NULL) != 0 ||
        pthread_create(&processor_tid, NULL, processor_thread, NULL) != 0 ||
        pthread_create(&sensor_tid, NULL, sensor_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create worker thread\n");
        interrupted = 1;
        cleanup();
        return 1;
    }

    printf("SMS started: interval=%ums temp_threshold=%.1f humidity_threshold=%.1f duration=%us\n",
           interval_ms, temp_threshold, humidity_threshold, duration_sec);

    time_t start = time(NULL);
    while (!interrupted) {
        if (duration_sec && time(NULL) - start >= duration_sec) {
            interrupted = 1;
            break;
        }
        struct timespec pause = {0, 100000000};
        nanosleep(&pause, NULL);
    }

    pthread_mutex_lock(&buffer->mutex);
    buffer->stopping = 1;
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    pthread_join(sensor_tid, NULL);
    pthread_join(processor_tid, NULL);

    /* Wake blocked queue consumers so they can exit cleanly. */
    Message stop_msg = {{0}};
    snprintf(stop_msg.text, sizeof(stop_msg.text), "SHUTDOWN");
    mq_send(alarm_q, (char *)&stop_msg, sizeof(stop_msg), 0);
    mq_send(log_q, (char *)&stop_msg, sizeof(stop_msg), 0);

    pthread_join(alert_tid, NULL);
    pthread_join(logger_tid, NULL);

    pthread_mutex_lock(&state_mutex);
    fprintf(stderr, "Alerts total: TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu\n",
            alerts_temp, alerts_hum, alerts_spike);
    pthread_mutex_unlock(&state_mutex);

    cleanup();
    return 0;

bad_args:
    usage(argv[0]);
    return 2;
}
