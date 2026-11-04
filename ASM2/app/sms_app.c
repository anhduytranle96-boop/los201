#include "circular_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

static CircularBuffer buffer;
static volatile sig_atomic_t stop_requested = 0;
static volatile sig_atomic_t producer_done = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static void sleep_ms(long milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

static void *producer_thread(void *arg)
{
    (void)arg;
    char sample[SAMPLE_LENGTH];

    while (!stop_requested) {
        int fd = open("/dev/sms_sensor", O_RDONLY);

        if (fd < 0) {
            perror("open sensor");
            sleep_ms(500);
            continue;
        }

        ssize_t n = read(fd, sample, sizeof(sample) - 1);
        close(fd);

        if (n > 0) {
            sample[n] = '\0';
            buffer_put(&buffer, sample);
        }

        sleep_ms(500);
    }

    producer_done = 1;
    return NULL;
}

static void *consumer_thread(void *arg)
{
    (void)arg;
    char sample[SAMPLE_LENGTH];

    while (!producer_done || current_size(&buffer) > 0) {
        if (buffer_get(&buffer, sample, sizeof(sample)) == 1) {
            /* Sample consumed. */
        } else {
            sleep_ms(20);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    long duration = 0;

    if (argc == 3 && strcmp(argv[1], "-d") == 0) {
        char *end;
        duration = strtol(argv[2], &end, 10);
        if (*end != '\0' || duration < 0) {
            fprintf(stderr, "Invalid duration\n");
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [-d seconds]\n", argv[0]);
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    if (buffer_init(&buffer) != 0) {
        fprintf(stderr, "Buffer initialization failed\n");
        return 1;
    }

    pthread_t producer, consumer;

    if (pthread_create(&producer, NULL, producer_thread, NULL) != 0) {
        perror("pthread_create producer");
        buffer_destroy(&buffer);
        return 1;
    }

    if (pthread_create(&consumer, NULL, consumer_thread, NULL) != 0) {
        perror("pthread_create consumer");
        stop_requested = 1;
        pthread_join(producer, NULL);
        buffer_destroy(&buffer);
        return 1;
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (!stop_requested) {
        if (duration > 0) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            long elapsed = now.tv_sec - start.tv_sec;
            if (elapsed >= duration)
                break;
        }
        sleep_ms(100);
    }

    stop_requested = 1;
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    unsigned long produced = total_produced(&buffer);
    unsigned long consumed = total_consumed(&buffer);
    unsigned long size = current_size(&buffer);
    unsigned long overflows = overflow_count(&buffer);

    printf("[SUMMARY] produced=%lu consumed=%lu size=%lu overflow=%lu\n",
           produced, consumed, size, overflows);

    buffer_destroy(&buffer);
    return 0;
}
