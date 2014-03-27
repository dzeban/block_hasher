/* 
 * block_hasher - Hash device blocks in multiple threads
 *
 * This file contains __testing__ program `block_hasher` that reads block device
 * in multiple threads and calculates hashes on blocks of given size.
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
#include <time.h>

#include <pthread.h>
#include <openssl/evp.h>

#define OPTSTR "d:b:t:n:h"
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
            free(job_info);\
            pthread_attr_destroy( &tattr );\
            return EXIT_FAILURE;\
        }

#define get_clock( t ) \
    err = clock_gettime( CLOCK_MONOTONIC, t );\
    if( err )\
    {\
        perror("clock_gettime");\
        pthread_exit(NULL);\
    }

// Store info about opened block device
struct block_device
{
    int fd;
    off_t size; // in bytes
};

// Information for thread. 
// Used as thread function argument.
struct job
{
    pthread_t id;
    int num;
    off_t off;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    double bps; // Bytes per sec
};

// Global parameters set from arguments
int block_size = -1;
int num_threads = -1;
int num_blocks = -1;

// Shared variables
struct block_device *bdev;
FILE *file;
pthread_mutex_t mutex;

// ============================================================================

void usage()
{
    printf("usage: block_summer -d <device> -b <block size> -t <thread number> -n [blocks number]\n");
}

double time_diff(struct timespec start, struct timespec end)
{
    struct timespec diff;
    double sec;

    if ( (end.tv_nsec - start.tv_nsec) < 0 ) 
    {
        diff.tv_sec  = end.tv_sec - start.tv_sec - 1;
        diff.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
    } 
    else 
    {
        diff.tv_sec  = end.tv_sec - start.tv_sec;
        diff.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    sec = (double)diff.tv_nsec / 1000000000 + diff.tv_sec;

    return sec;
}

// ============================================================================

void *thread_func(void *arg)
{
    struct job *j = arg;
    long total_blocks, nblocks, i;
    off_t offset, gap;
    char *buf;
    int err;
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;
    struct timespec start, end;
    double sec_diff;
    unsigned long long bytes = 0;

    buf = malloc(block_size);
    if( !buf )
    {
        fprintf(stderr, "Failed to allocate buffers\n");
        pthread_exit(NULL);
    }

    if( num_blocks != -1 )
    {
        nblocks = num_blocks;
    }
    else
    {
        total_blocks = bdev->size / block_size;
        nblocks = total_blocks / num_threads;
    }

    gap = num_threads * block_size; // Multiply here to avoid integer overflow

    // Initialize EVP and start reading
    pthread_mutex_lock( &mutex );
    md = EVP_sha1();
    mdctx = EVP_MD_CTX_create();
    EVP_DigestInit_ex( mdctx, md, NULL );
    pthread_mutex_unlock( &mutex );

    get_clock( &start );
    for( i = 0; i < nblocks; i++)
    {
        offset = j->off + gap * i;

        // Read at offset without changing file pointer
        err = pread( bdev->fd, buf, block_size, offset );
        if( err == -1 )
        {
            fprintf(stderr, "T%02d Failed to read at %zu\n", j->num, offset);
            perror("pread");
            pthread_exit(NULL);
        }

        bytes += err; // On success pread returns bytes read

        // Update digest
        EVP_DigestUpdate( mdctx, buf, block_size );
    }
    get_clock( &end );
    sec_diff = time_diff( start, end );

    EVP_DigestFinal_ex( mdctx, j->digest, &j->digest_len );
    EVP_MD_CTX_destroy(mdctx);

    // Write job summary
    pthread_mutex_lock( &mutex );
    i = 0;
    fprintf(file, "T%02d: ", j->num);
    fprintf(file, "%.2f MB/s ", (double)bytes / sec_diff / 1000000);
    while(i++ < j->digest_len) 
    {
        fprintf(file, "%02x", j->digest[i]);
    }
    fprintf(file, "\n");
    pthread_mutex_unlock( &mutex );

    free(buf);
    return NULL;
}

// ============================================================================

struct block_device *bdev_open( char *dev_path )
{
    struct block_device *dev;
    dev = malloc(sizeof(*dev));

    dev->fd = open( dev_path, O_RDONLY );
    if( dev->fd == -1 )
    {
        perror("open");
        return 0;
    }

    dev->size = lseek(dev->fd, 0, SEEK_END);
    if( dev->size == -1 )
    {
        perror("lseek");
        return NULL;
    }

    return dev;
}

// ============================================================================

void bdev_close( struct block_device *dev )
{
    int err;

    err = close( dev->fd );
    if( err == -1 )
    {
        perror("close");
    }

    free(dev);
    dev = NULL;

    return;
}

// ============================================================================

int main(int argc, char *argv[])
{
    char dev_path[DEV_PATH_LEN];

    bool dev_set = false, block_size_set = false, num_threads_set = false;
    char *errptr;
    struct job *job_info;
    pthread_attr_t tattr;
    int err;
    int opt;
    int i;

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

            case 'n':
                num_blocks = strtol(optarg, &errptr, 10);
                strtol_handle(num_blocks);
                break;

            case 'h':
                usage();
                return EXIT_SUCCESS;

            default:
                usage();
                return EXIT_FAILURE;
        }
    }
    // Check mandatory options
    if( !( dev_set && block_size_set && num_threads_set ) )
    {
        usage();
        return EXIT_FAILURE;
    }

    // ------------------------
    // Open block device
    // ------------------------
    bdev = bdev_open(dev_path);
    if( bdev == NULL)
    {
        return EXIT_FAILURE;
    }
    
    // ------------------------
    // Open output file
    // ------------------------
    file = fopen("digest.out", "w");
    if( file == NULL )
    {
        perror("fopen");
        return EXIT_FAILURE;
    }

    // --------------------------------------------------------
    // Create and run threads
    // --------------------------------------------------------
    job_info = calloc( num_threads, sizeof(struct job) );
    if( job_info == NULL )
    {
        fprintf(stderr, "Failed to allocate memory\n");
        free(job_info);
        return EXIT_FAILURE;
    }

    err = pthread_attr_init( &tattr );
    pthread_handle(err, "pthread_attr_init");

    err = pthread_mutex_init(&mutex, NULL);
    pthread_handle(err, "pthread_mutex_init");

    for( i = 0; i < num_threads; i++ )
    {
        job_info[i].num = i;
        job_info[i].off = i * block_size;
        
        err = pthread_create( &job_info[i].id, 
                              &tattr, 
                              &thread_func,
                              &job_info[i] );
        pthread_handle(err, "pthread_create");
    }

    // Wait for threads to join
    for( i = 0; i < num_threads; i++ )
    {
        err = pthread_join( job_info[i].id, NULL );
        pthread_handle(err, "pthread_join");
    }

    // Housekeeping
    bdev_close( bdev );
    fclose(file);
    free(job_info);
    pthread_attr_destroy( &tattr );
    return EXIT_SUCCESS;
}
