/*
 The MIT License (MIT)
 Copyright (c) 2011 Jayant Sai (@j6y6nt)
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <pthread.h>
#include <semaphore.h>

#include "Connection.h"

#define kDataSize 512
#define kReadSize 255
#define kSentenceSize 255

static int fd;
static pthread_t ioThread;

static bool runThread;
sem_t threadSem;


/* private functions */
static int strchari(const char *haystack, char needle, int beg) {
    int i;
    size_t il;
    
    i = (int) beg;
    il = strlen(haystack);
    for (; i<il; i++) {
        if (haystack[i] == needle) {
            return i;
        }
    }
    
    return -1;
}

static void *_readNMEA(void *c) {
    char data[kDataSize], indata[kReadSize], sentence[kSentenceSize];
    char dollar, eol;
    int beg, end;
    size_t charsize = sizeof(char);
    bool run;
    
    dollar = '$';
    eol = '\r';
    
    fcntl(fd, F_SETFL, 0);
    
    sem_wait(&threadSem);
    run = runThread;
    sem_post(&threadSem);
    
    while (run) { // while (true)
        read(fd, indata, kReadSize);
        printf("\n<io>\n%s\n</io>\n", indata);
        
        beg = 0;
        memset(data, 0, kDataSize);
        strcat(data, sentence);
        strcat(data, indata);
        memset(sentence, 0, kSentenceSize);
        
        while (true) {
            beg = strchari(data, dollar, beg);
            end = strchari(data, eol, beg+1);
            
            if (beg == -1) {
                break;
            }
            else if (end == -1) {
                end = (int) strlen(data);
                memset(sentence, 0, kSentenceSize);
                memcpy(sentence, data + (beg * charsize), end - beg);
                break;
            }
            else {
                memcpy(sentence, data + (beg * charsize), end - beg);
                printf("<sentence>%s</sentence>\n", sentence);
                memset(sentence, 0, kSentenceSize);
            }
            
            beg++;
        }
        
        sem_wait(&threadSem);
        run = runThread;
        sem_post(&threadSem);
    }
    
    pthread_exit(0);
}

/* public functions */
bool connectSerial(const char *filename) {
    unsigned count = 1;
    
    fd = open(filename, O_RDONLY | O_NOCTTY | O_NDELAY);
    
    while (fd == -1) {
        fd = open(filename, O_RDONLY | O_NOCTTY | O_NDELAY);
        printf("count = %u, fd = %i\n", count, fd);
        
        if (fd != -1 || count >= 3) {
            break;
        }
        
        close(fd);
        sleep(1);
        count++;
    }
    
    printf("fd = %i\n", fd);
    
    if (fd != -1) {
        return true;
    }
    
    return false;
}

void readNMEA(void) {
    int rc;
    pthread_attr_t attr;
    
    if (ioThread == NULL) {
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        
        threadSem = sem_init(&threadSem, 0, 1);
        sem_wait(&threadSem);
        runThread = true;
        sem_post(&threadSem);
        
        rc = pthread_create(&ioThread, &attr, _readNMEA, NULL);
        
        pthread_attr_destroy(&attr);
        
        if (rc) {
            printf("Unable to create io thread.\n");
            exit(-1);
        }
    }
}

void disconnectSerial(void) {
    void *status;
    
    sem_wait(&threadSem);
    runThread = false;
    sem_post(&threadSem);
    
    pthread_join(ioThread, &status);
    sem_destroy(&threadSem);

    close(fd);
    
    pthread_exit(NULL);
}