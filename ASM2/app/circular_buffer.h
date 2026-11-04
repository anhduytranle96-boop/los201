#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stddef.h>
#include <pthread.h>

#define BUFFER_CAPACITY 16
#define SAMPLE_LENGTH 128

typedef struct {
    char data[BUFFER_CAPACITY][SAMPLE_LENGTH];
    size_t head;
    size_t tail;
    size_t size;
    unsigned long produced;
    unsigned long consumed;
    unsigned long overflows;
    pthread_mutex_t mutex;
} CircularBuffer;

int buffer_init(CircularBuffer *buffer);
void buffer_destroy(CircularBuffer *buffer);
int buffer_put(CircularBuffer *buffer, const char *sample);
int buffer_get(CircularBuffer *buffer, char *sample, size_t sample_size);

unsigned long total_produced(CircularBuffer *buffer);
unsigned long total_consumed(CircularBuffer *buffer);
unsigned long current_size(CircularBuffer *buffer);
unsigned long overflow_count(CircularBuffer *buffer);

#endif
