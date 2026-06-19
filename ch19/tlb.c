#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

double get_time_in_seconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_pages> <num_trials>\n", argv[0]);
        exit(1);
    }

    int num_pages = atoi(argv[1]);
    int num_trials = atoi(argv[2]);

    if (num_pages <= 0 || num_trials <= 0) {
        fprintf(stderr, "num_pages and num_trials must be positive.\n");
        exit(1);
    }

    int page_size = getpagesize();
    int jump = page_size / sizeof(int);

    int total_ints = num_pages * jump;

    int *a = (int *)malloc(total_ints * sizeof(int));
    if (a == NULL) {
        perror("malloc");
        exit(1);
    }

    // 初始化数组，避免第一次访问时全是缺页异常的影响
    for (int i = 0; i < total_ints; i++) {
        a[i] = 0;
    }

    // 预热：先访问一遍，尽量减少首次访问、缺页等影响
    for (int i = 0; i < total_ints; i += jump) {
        a[i] += 1;
    }

    double start = get_time_in_seconds();

    for (int t = 0; t < num_trials; t++) {
        for (int i = 0; i < total_ints; i += jump) {
            a[i] += 1;
        }
    }

    double end = get_time_in_seconds();

    double elapsed = end - start;
    long long total_accesses = (long long) * num_trials;
    double time_per_access_ns = elapsed * 1e9 / total_accesses;

    printf("Number of pages: %d\n", num_pages);
    printf("Number of trials: %d\n", num_trials);
    printf("Page size: %d bytes\n", page_size);
    printf("Total time: %.6f seconds\n", elapsed);
    printf("Average time per access: %.2f ns\n", time_per_access_ns);

    free(a);
    return 0;
}