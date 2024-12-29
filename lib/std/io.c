#include "mocha.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

null __io__read__(i32 fd, str *buf, i64 count) {
    if (buf->heap && buf->ptr) {
        free(buf->ptr);
        buf->ptr = NULL;
        buf->len = 0;
    }

    buf->heap = true;  // Mark the buffer as heap-allocated

    if (count == -1) {
        size_t capacity = 64;
        buf->ptr = malloc(capacity);
        if (!buf->ptr) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        buf->len = 0;

        char c;
        ssize_t n;
        while ((n = read(fd, &c, 1)) > 0) {
            if (c == '\n' || c == '\r')
                break;

            if (buf->len + 1 >= capacity) {
                capacity *= 2; // Double the capacity
                char *new_ptr = realloc(buf->ptr, capacity);
                if (!new_ptr) {
                    perror("realloc");
                    free(buf->ptr);
                    exit(EXIT_FAILURE);
                }
                buf->ptr = new_ptr;
            }

            buf->ptr[buf->len++] = c;
        }

        if (n < 0) {
            perror("read");
            free(buf->ptr);
            buf->ptr = NULL;
            buf->len = 0;
            exit(EXIT_FAILURE);
        }

        buf->ptr = realloc(buf->ptr, buf->len + 1); // Adjust to actual size
        buf->ptr[buf->len] = '\0';
    } else {
        buf->ptr = malloc(count);
        if (!buf->ptr) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        buf->len = read(fd, buf->ptr, count);

        if (buf->len < 0) {
            perror("read");
            free(buf->ptr);
            buf->ptr = NULL;
            buf->len = 0;
            exit(EXIT_FAILURE);
        }
    }
}

i64 __io__write__(i32 fd, str *buf, i64 count)
{
    return write(fd, buf->ptr, count);
}

str *__io__popen__(str *prog, str *mode)
{
    char _prog[prog->len];
    memcpy(_prog, prog->ptr, prog->len);
    _prog[prog->len] = 0;

    char _mode[mode->len];
    memcpy(_mode, mode->ptr, mode->len);
    _mode[mode->len] = 0;

    FILE *pipe = popen(_prog, _mode);

    if (pipe == NULL) {
        perror("popen");
        return NULL;
    }

    str *result = malloc(sizeof(str));
    if (result == NULL) {
        perror("malloc");
        pclose(pipe);
        return NULL;
    }

    result->ptr = NULL;
    result->len = 0;
    result->heap = true;

    size_t capacity = 1024;
    result->ptr = malloc(capacity);

    if (result->ptr == NULL) {
        perror("malloc");
        free(result);
        pclose(pipe);
        return NULL;
    }

    size_t n;
    while ((n = fread(result->ptr + result->len, 1, capacity - result->len, pipe)) > 0) {
        result->len += n;

        if (result->len == capacity) {
            capacity *= 2;
            char *new_ptr = realloc(result->ptr, capacity);
            if (!new_ptr) {
                perror("realloc");
                free(result->ptr);
                free(result);
                pclose(pipe);
                return NULL;
            }
            result->ptr = new_ptr;
        }
    }

    if (ferror(pipe)) {
        perror("fread");
        free(result->ptr);
        free(result);
        pclose(pipe);
        return NULL;
    }

    result->ptr[result->len] = '\0';
    pclose(pipe);

    return result;
}

null __io__flush__(i32 fd)
{
    fsync(fd);
}
