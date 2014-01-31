/* 
 * block_hasher - Hash device blocks in multiple threads
 *
 * This file contains __testing__ program `block_hasher` that reads block device
 * in multiple threads and calculates hashes on blocks of given size.
 *
 * Usage:
 * ./block_hasher -d <device> -b <block size> -t <thread number>
 *  
 * Copyright (c) 2014 Alex Dzyoba <avd@reduct.ru>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.*
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>

#include <pthread.h>
#include <openssl/sha.h>

#define OPTSTR "d:b:t:h"
#define DEV_PATH_LEN 64

// strtol error handling
#define strtol_handle(x) \
        if( *errptr != 0 || errno == ERANGE) \
        {\
            fprintf(stderr, "Argument invalid\n");\
            return EXIT_FAILURE;\
        }

// pthread functions error handling
#define pthread_handle(err, str) \
        if( err )\
        {\
            errno = err;\
            perror(str);\
            free(thread_info);\
            pthread_attr_destroy( &tattr );\
            return EXIT_FAILURE;\
        }

// Store info about opened block device
struct block_device
{
    int fd;
    off_t size; // in bytes
};

// Information for thread. 
// Used as thread function argument.
struct thread
{
    pthread_t id;
    struct block_device *bdev;
    int num;
    off_t off;
    int block_size;
    int num_threads;
};

// ============================================================================

void usage()
{
    printf("usage: block_summer -d <device> -b <block size> -t <thread number>\n");
}

// ============================================================================

void *thread_func(void *arg)
{
    struct thread *t = arg;
    long nblocks, nset, i;
    off_t offset, gap;
    unsigned char *buf;
    int err;

    buf = malloc(t->block_size);
    if( !buf )
    {
        fprintf(stderr, "Failed to allocate buffers\n");
        pthread_exit(NULL);
    }

    nblocks = t->bdev->size / t->block_size;
    nset = nblocks / t->num_threads;

    gap = t->num_threads * t->block_size; // Multiply here to avoid integer overflow
    for( i = 0; i < nset; i++)
    {
        offset = t->off + gap * i;
        err = pread( t->bdev->fd, buf, t->block_size, offset );
        if( err == -1 )
        {
            fprintf(stderr, "T%02d Failed to read at %llu\n", t->num, (unsigned long long)offset);
            perror("pread");
            pthread_exit(NULL);
        }

        SHA1(buf, t->block_size, NULL);
    }

    free(buf);
    return NULL;
}

// ============================================================================

struct block_device *bdev_open( char *dev_path )
{
    struct block_device *bdev;
    bdev = malloc(sizeof(*bdev));

    bdev->fd = open( dev_path, O_RDONLY );
    if( bdev->fd == -1 )
    {
        perror("open");
        return 0;
    }

    bdev->size = lseek(bdev->fd, 0, SEEK_END);
    if( bdev->size == -1 )
    {
        perror("lseek");
        return NULL;
    }

    return bdev;
}

// ============================================================================

void bdev_close( struct block_device *bdev )
{
    int err;

    err = close( bdev->fd );
    if( err == -1 )
    {
        perror("close");
    }

    return;
}

// ============================================================================

int main(int argc, char *argv[])
{
    int opt;
    struct block_device *bdev;
    char dev_path[DEV_PATH_LEN];
    long block_size = 0;
    long num_threads = 0;
    bool dev_set = false, block_size_set = false, num_threads_set = false;
    char *errptr;
    struct thread *thread_info;
    pthread_attr_t tattr;
    int i;
    int err;

    // --------------------------------------------
    // Parse arguments
    // --------------------------------------------
    while((opt = getopt(argc, argv, OPTSTR)) != -1)
    {
        switch(opt)
        {
            case 'd':
                strncpy(dev_path, optarg, DEV_PATH_LEN);
                dev_set = true;
                break;

            case 'b':
                block_size = strtol(optarg, &errptr, 10);
                strtol_handle(block_size);
                block_size_set = true;
                break;

            case 't':
                num_threads = strtol(optarg, &errptr, 10);
                strtol_handle(num_threads);
                num_threads_set = true;
                break;

            case 'h':
                usage();
                return EXIT_SUCCESS;

            default:
                usage();
                return EXIT_FAILURE;
        }
    }
    // All options must be set
    if( !( dev_set && block_size_set && num_threads_set ) )
    {
        usage();
        return EXIT_FAILURE;
    }
    printf("dev_path %s, block_size %ld, num_threads %ld\n", dev_path, block_size, num_threads);

    // ------------------------
    // Open block device
    // ------------------------
    bdev = bdev_open(dev_path);
    if( bdev == NULL)
    {
        return EXIT_FAILURE;
    }
    printf("bdev: %d, %zu\n", bdev->fd, bdev->size);

    // --------------------------------------------------------
    // Create and run threads
    // --------------------------------------------------------
    thread_info = calloc( num_threads, sizeof(struct thread) );
    if( thread_info == NULL )
    {
        fprintf(stderr, "Failed to allocate memory\n");
        free(thread_info);
        return EXIT_FAILURE;
    }

    err = pthread_attr_init( &tattr );
    pthread_handle(err, "pthread_attr_init");

    for( i = 0; i < num_threads; i++ )
    {
        thread_info[i].bdev = bdev;
        thread_info[i].num = i;
        thread_info[i].off = i * block_size;
        thread_info[i].block_size = block_size;
        thread_info[i].num_threads = num_threads;
        
        err = pthread_create( &thread_info[i].id, 
                              &tattr, 
                              &thread_func,
                              &thread_info[i] );
        pthread_handle(err, "pthread_create");
    }

    // Wait for threads to join
    for( i = 0; i < num_threads; i++ )
    {
        err = pthread_join( thread_info[i].id, NULL );
        pthread_handle(err, "pthread_join");

        printf("Thread %d joined\n", i);
    }

    // Housekeeping
    bdev_close( bdev );
    free(thread_info);
    pthread_attr_destroy( &tattr );
    return EXIT_SUCCESS;
}
