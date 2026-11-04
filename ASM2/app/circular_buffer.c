#include "circular_buffer.h"
#include <string.h>

int buffer_init(CircularBuffer *buffer)
{
    memset(buffer, 0, sizeof(*buffer));
    return pthread_mutex_init(&buffer->mutex, NULL);
}

void buffer_destroy(CircularBuffer *buffer)
{
    pthread_mutex_destroy(&buffer->mutex);
}

int buffer_put(CircularBuffer *buffer, const char *sample)
{
    pthread_mutex_lock(&buffer->mutex);

    if (buffer->size == BUFFER_CAPACITY) {
        buffer->overflows++;
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    strncpy(buffer->data[buffer->tail], sample, SAMPLE_LENGTH - 1);
    buffer->data[buffer->tail][SAMPLE_LENGTH - 1] = '\0';

    buffer->tail = (buffer->tail + 1) % BUFFER_CAPACITY;
    buffer->size++;
    buffer->produced++;

    pthread_mutex_unlock(&buffer->mutex);
    return 0;
}

int buffer_get(CircularBuffer *buffer, char *sample, size_t sample_size)
{
    if (sample_size == 0)
        return 0;

    pthread_mutex_lock(&buffer->mutex);

    if (buffer->size == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return 0;
    }

    strncpy(sample, buffer->data[buffer->head], sample_size - 1);
    sample[sample_size - 1] = '\0';

    buffer->head = (buffer->head + 1) % BUFFER_CAPACITY;
    buffer->size--;
    buffer->consumed++;

    pthread_mutex_unlock(&buffer->mutex);
    return 1;
}

unsigned long total_produced(CircularBuffer *buffer)
{
    unsigned long value;
    pthread_mutex_lock(&buffer->mutex);
    value = buffer->produced;
    pthread_mutex_unlock(&buffer->mutex);
    return value;
}

unsigned long total_consumed(CircularBuffer *buffer)
{
    unsigned long value;
    pthread_mutex_lock(&buffer->mutex);
    value = buffer->consumed;
    pthread_mutex_unlock(&buffer->mutex);
    return value;
}

unsigned long current_size(CircularBuffer *buffer)
{
    unsigned long value;
    pthread_mutex_lock(&buffer->mutex);
    value = buffer->size;
    pthread_mutex_unlock(&buffer->mutex);
    return value;
}

unsigned long overflow_count(CircularBuffer *buffer)
{
    unsigned long value;
    pthread_mutex_lock(&buffer->mutex);
    value = buffer->overflows;
    pthread_mutex_unlock(&buffer->mutex);
    return value;
}
