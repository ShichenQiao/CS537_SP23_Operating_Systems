#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define MAX_SIZE 50000000

typedef struct {
    int key;
    char* index;
} KeyIndex;

KeyIndex keys[MAX_SIZE];
KeyIndex temp[MAX_SIZE];

typedef struct {
    int thread_id;
    int start;
    int end;
} SortParams;

void merge(int start, int mid, int end) {
    int i = start;
    int j = mid;
    int k = start;

    while (i < mid && j < end) {
        if (keys[i].key <= keys[j].key)
            temp[k++] = keys[i++];
        else
            temp[k++] = keys[j++];
    }

    while (i < mid)
        temp[k++] = keys[i++];

    while (j < end)
        temp[k++] = keys[j++];

    for (int i = start; i < end; i++)
        keys[i] = temp[i];
}

void merge_sort(int start, int end) {
    if (end - start < 2)
        return;

    int mid = start + (end - start) / 2;
    merge_sort(start, mid);
    merge_sort(mid, end);
    merge(start, mid, end);
}

void* sort_thread(void* arg) {
    SortParams* params = (SortParams*)arg;
    int start = params->start;
    int end = params->end;
    merge_sort(start, end);
    pthread_exit(NULL);
}

int main(int argc, char** argv) {
    int numThreads = atoi(argv[3]);
    struct stat statbuf;
    int fdin, fdout;
    char *in, *out;

    int mode = S_IRWXU | S_IRWXG | S_IRWXO;

    if ((fdin = open(argv[1], O_RDONLY)) < 0) {
        printf("error reading");
        return 0;
    }

    if (fstat (fdin,&statbuf) < 0) {
        printf ("error extracting file size");
        return 0;
    }

    in = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fdin, 0);
    if (in == MAP_FAILED) {
        printf ("mmap input error");
        return 0;
    }

    close(fdin);

    int numRecords = statbuf.st_size / 100; // number of records in the file
    for (int i = 0; i < numRecords; i++) {
        keys[i].key = *(int*)in;
        keys[i].index = in;
        in += 100;
    }

/*
    FILE * timelog = fopen("time.log", "w");
    clock_t t;
    t = clock();
*/

    pthread_t threads[numThreads];
    SortParams params[numThreads];

    for (int i = 0; i < numThreads; i++) {
        params[i].thread_id = i;
        params[i].start = i * (numRecords / numThreads);
        params[i].end = (i == numThreads - 1) ? numRecords : (i + 1) * (numRecords / numThreads);
        pthread_create(&threads[i], NULL, sort_thread, &params[i]);
    }

    for (int i = 0; i < numThreads; i++)
        pthread_join(threads[i], NULL);

    // Perform final merge across all threads
    for (int i = 0; i < numThreads; i++) {
        //printf("%i, %i\n", params[i].start, params[i].end);
        merge(0, params[i].start, params[i].end);
    }

/*
    // Verify sorted keys
    for (int i = 0; i < numRecords; i++)
        printf("%i ", keys[i]);
    for (int i = 0; i < numRecords - 1; i++) {
        if (keys[i].key > keys[i+1].key) {
            printf("Error: keys not sorted!\n");
            break;
        }
    }
*/

/*
    t = clock() - t;
    double time_taken = ((double)t)/CLOCKS_PER_SEC;
    fprintf(timelog, "%f\n", time_taken);
    //printf("execution time = %f seconds\n", time_taken);
    fclose(timelog);
*/

    if ((fdout = open (argv[2], O_RDWR | O_CREAT | O_TRUNC, mode)) < 0) {
        printf ("error writing");
        return 0;
    }

    if (lseek (fdout, statbuf.st_size - 1, SEEK_SET) == -1) {
        printf ("lseek error\n");
        return 0;
    }

    if (write (fdout, "", 1) != 1) {
        printf ("write error");
        return 0;
    }

    if ((out = mmap(0, statbuf.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fdout, 0)) == (void*)-1) {
        printf ("mmap output error\n");
        return 0;
    }

    for (int i = 0; i < numRecords; i++) {
        memcpy(out, keys[i].index, 100);
        out += 100;
    }

    fsync(fdout);
    munmap(in,statbuf.st_size);
    munmap(out,statbuf.st_size);
    close(fdout);

    return 0;
}
