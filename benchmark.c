

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <errno.h>
#include <stdint.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static volatile size_t sink_size;
static volatile void *sink_ptr;
static volatile int sink_int;
static volatile double sink_double;

typedef struct
{
    const char *name;
    void (*init)(void **state);
    size_t (*run)(void *state, size_t iters);
    void (*cleanup)(void *state);
} Benchmark;

/* Time utility */
static inline uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ---------------- Benchmarks ---------------- */

/* 1. strlen */
static void init_strlen(void **state)
{
    char *s = malloc(1025);
    for (int i = 0; i < 1024; i++)
        s[i] = 'A' + (i % 26);
    s[1024] = '\0';
    *state = s;
}
static size_t run_strlen(void *state, size_t iters)
{
    char *s = (char *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        size_t len = strlen(s);
        sink_size = len;
        ops++;
    }
    return ops;
}
static void cleanup_free(void *state) { free(state); }

/* 2. strcmp */
typedef struct
{
    char *a;
    char *b;
} strcmp_state;
static void init_strcmp(void **state)
{
    strcmp_state *st = malloc(sizeof(*st));
    st->a = strdup("The quick brown fox jumps over the lazy dog 1234567890");
    st->b = strdup("The quick brown fox jumps over the lazy dog 1234567890");
    *state = st;
}
static size_t run_strcmp(void *state, size_t iters)
{
    strcmp_state *st = (strcmp_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        int r = strcmp(st->a, st->b);
        sink_int = r;
        ops++;
    }
    return ops;
}
static void cleanup_strcmp(void *state)
{
    strcmp_state *st = (strcmp_state *)state;
    free(st->a);
    free(st->b);
    free(st);
}

/* 3. strcpy */
typedef struct
{
    char *src;
    char *dst;
} strcpy_state;
static void init_strcpy(void **state)
{
    strcpy_state *st = malloc(sizeof(*st));
    st->src = malloc(2048);
    for (int i = 0; i < 2047; i++)
        st->src[i] = 'a' + (i % 26);
    st->src[2047] = '\0';
    st->dst = malloc(2048);
    *state = st;
}
static size_t run_strcpy(void *state, size_t iters)
{
    strcpy_state *st = (strcpy_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        char *r = strcpy(st->dst, st->src);
        sink_ptr = r;
        ops++;
    }
    return ops;
}
static void cleanup_strcpy(void *state)
{
    strcpy_state *st = (strcpy_state *)state;
    free(st->src);
    free(st->dst);
    free(st);
}

/* 4. strcat */
typedef struct
{
    char *piece;
    char *buf;
} strcat_state;
static void init_strcat(void **state)
{
    strcat_state *st = malloc(sizeof(*st));
    st->piece = strdup("segment1234567890");
    st->buf = malloc(1024);
    *state = st;
}
static size_t run_strcat(void *state, size_t iters)
{
    strcat_state *st = (strcat_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        st->buf[0] = '\0';
        for (int k = 0; k < 16; k++)
        {
            char *r = strcat(st->buf, st->piece);
            sink_ptr = r;
            ops++;
        }
    }
    return ops;
}
static void cleanup_strcat(void *state)
{
    strcat_state *st = (strcat_state *)state;
    free(st->piece);
    free(st->buf);
    free(st);
}

/* 5. strchr */
typedef struct
{
    char *s;
} strchr_state;
static void init_strchr(void **state)
{
    strchr_state *st = malloc(sizeof(*st));
    st->s = malloc(4097);
    for (int i = 0; i < 4096; i++)
        st->s[i] = 'a' + (i % 26);
    st->s[4096] = '\0';
    *state = st;
}
static size_t run_strchr(void *state, size_t iters)
{
    strchr_state *st = (strchr_state *)state;
    /* Vary target each iteration so the call can't be constant-folded. */
    static volatile char targets[] = "abcdefghijklmnopqrstuvwxyz";
    size_t ops = 0;
    int acc = 0;
    for (size_t i = 0; i < iters; i++)
    {
        char target = targets[i % (sizeof(targets) - 1)];
        const char *r = strchr(st->s, target);
        /* Consume result in a way the compiler can't predict fully. */
        if (r)
            acc += (int)(r - st->s);
        else
            acc -= 1;
        ops++;
    }
    sink_int = acc;
    return ops;
}
static void cleanup_strchr(void *state)
{
    strchr_state *st = (strchr_state *)state;
    free(st->s);
    free(st);
}

/* 6. memcmp */
typedef struct
{
    unsigned char *a, *b;
    size_t len;
} memcmp_state;
static void init_memcmp(void **state)
{
    memcmp_state *st = malloc(sizeof(*st));
    st->len = 8192;
    st->a = malloc(st->len);
    st->b = malloc(st->len);
    for (size_t i = 0; i < st->len; i++)
    {
        st->a[i] = (unsigned char)(i & 0xFF);
        st->b[i] = st->a[i];
    }
    *state = st;
}
static size_t run_memcmp(void *state, size_t iters)
{
    memcmp_state *st = (memcmp_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        int r = memcmp(st->a, st->b, st->len);
        sink_int = r;
        ops++;
    }
    return ops;
}
static void cleanup_memcmp(void *state)
{
    memcmp_state *st = (memcmp_state *)state;
    free(st->a);
    free(st->b);
    free(st);
}

/* 7. memcpy */
typedef struct
{
    unsigned char *src, *dst;
    size_t len;
} memcpy_state;
static void init_memcpy(void **state)
{
    memcpy_state *st = malloc(sizeof(*st));
    st->len = 16384;
    st->src = malloc(st->len);
    st->dst = malloc(st->len);
    for (size_t i = 0; i < st->len; i++)
        st->src[i] = (unsigned char)(rand());
    *state = st;
}
static size_t run_memcpy(void *state, size_t iters)
{
    memcpy_state *st = (memcpy_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        void *r = memcpy(st->dst, st->src, st->len);
        sink_ptr = r;
        ops++;
    }
    return ops;
}
static void cleanup_memcpy(void *state)
{
    memcpy_state *st = (memcpy_state *)state;
    free(st->src);
    free(st->dst);
    free(st);
}

/* 8. memmove (overlap) */
typedef struct
{
    unsigned char *buf;
    size_t len;
    size_t shift;
} memmove_state;
static void init_memmove(void **state)
{
    memmove_state *st = malloc(sizeof(*st));
    st->len = 16384;
    st->buf = malloc(st->len + 64);
    for (size_t i = 0; i < st->len + 64; i++)
        st->buf[i] = (unsigned char)i;
    st->shift = 32;
    *state = st;
}
static size_t run_memmove(void *state, size_t iters)
{
    memmove_state *st = (memmove_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        void *r = memmove(st->buf + st->shift, st->buf, st->len);
        sink_ptr = r;
        ops++;
    }
    return ops;
}
static void cleanup_memmove(void *state)
{
    memmove_state *st = (memmove_state *)state;
    free(st->buf);
    free(st);
}

/* 9. qsort */
typedef struct
{
    int *orig;
    int *work;
    size_t n;
} qsort_state;
static int qsort_cmp_int(const void *a, const void *b)
{
    int ia = *(const int *)a, ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}
static void init_qsort(void **state)
{
    qsort_state *st = malloc(sizeof(*st));
    st->n = 4096;
    st->orig = malloc(st->n * sizeof(int));
    st->work = malloc(st->n * sizeof(int));
    srand(1234);
    for (size_t i = 0; i < st->n; i++)
        st->orig[i] = rand();
    *state = st;
}
static size_t run_qsort(void *state, size_t iters)
{
    qsort_state *st = (qsort_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        memcpy(st->work, st->orig, st->n * sizeof(int));
        qsort(st->work, st->n, sizeof(int), qsort_cmp_int);
        sink_int = st->work[st->n / 2];
        ops++;
    }
    return ops;
}
static void cleanup_qsort(void *state)
{
    qsort_state *st = (qsort_state *)state;
    free(st->orig);
    free(st->work);
    free(st);
}

/* 10. bsearch */
typedef struct
{
    int *arr;
    int *keys;
    size_t n;
    size_t k;
} bsearch_state;
static void init_bsearch(void **state)
{
    bsearch_state *st = malloc(sizeof(*st));
    st->n = 4096;
    st->k = 128;
    st->arr = malloc(st->n * sizeof(int));
    for (size_t i = 0; i < st->n; i++)
        st->arr[i] = (int)(i * 2);
    st->keys = malloc(st->k * sizeof(int));
    for (size_t i = 0; i < st->k; i++)
        st->keys[i] = (int)((i * 7) % (st->n * 2));
    *state = st;
}
static size_t run_bsearch(void *state, size_t iters)
{
    bsearch_state *st = (bsearch_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->k; k++)
        {
            void *r = bsearch(&st->keys[k], st->arr, st->n, sizeof(int), qsort_cmp_int);
            sink_ptr = r;
            ops++;
        }
    }
    return ops;
}
static void cleanup_bsearch(void *state)
{
    bsearch_state *st = (bsearch_state *)state;
    free(st->arr);
    free(st->keys);
    free(st);
}

/* 11. malloc/free small */
static size_t run_malloc_small(void *state, size_t iters)
{
    (void)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        void *ptrs[256];
        for (int k = 0; k < 256; k++)
        {
            ptrs[k] = malloc(32);
            ops++;
        }
        for (int k = 0; k < 256; k++)
            free(ptrs[k]);
    }
    return ops;
}

/* 12. malloc/free medium */
static size_t run_malloc_medium(void *state, size_t iters)
{
    (void)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        void *ptrs[64];
        for (int k = 0; k < 64; k++)
        {
            ptrs[k] = malloc(4096);
            ops++;
        }
        for (int k = 0; k < 64; k++)
            free(ptrs[k]);
    }
    return ops;
}

/* 13. realloc pattern */
static size_t run_realloc_pattern(void *state, size_t iters)
{
    (void)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        size_t sz = 16;
        char *p = malloc(sz);
        for (int r = 0; r < 64; r++)
        {
            sz = (sz < 1024) ? sz * 2 : 16;
            char *np = realloc(p, sz);
            if (!np)
            {
                free(p);
                break;
            }
            p = np;
            ops++;
        }
        free(p);
    }
    return ops;
}

/* 14. sprintf int */
typedef struct
{
    char *buf;
    int *vals;
    size_t n;
} sprintf_int_state;
static void init_sprintf_int(void **state)
{
    sprintf_int_state *st = malloc(sizeof(*st));
    st->n = 256;
    st->vals = malloc(st->n * sizeof(int));
    for (size_t i = 0; i < st->n; i++)
        st->vals[i] = (int)(i * i + 12345);
    st->buf = malloc(32);
    *state = st;
}
static size_t run_sprintf_int(void *state, size_t iters)
{
    sprintf_int_state *st = (sprintf_int_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            int n = sprintf(st->buf, "%d", st->vals[k]);
            sink_int = n;
            ops++;
        }
    }
    return ops;
}
static void cleanup_sprintf_int(void *state)
{
    sprintf_int_state *st = (sprintf_int_state *)state;
    free(st->vals);
    free(st->buf);
    free(st);
}

/* 15. sprintf float */
typedef struct
{
    char *buf;
    double *vals;
    size_t n;
} sprintf_float_state;
static void init_sprintf_float(void **state)
{
    sprintf_float_state *st = malloc(sizeof(*st));
    st->n = 128;
    st->vals = malloc(st->n * sizeof(double));
    for (size_t i = 0; i < st->n; i++)
        st->vals[i] = (double)i / 3.14159;
    st->buf = malloc(64);
    *state = st;
}
static size_t run_sprintf_float(void *state, size_t iters)
{
    sprintf_float_state *st = (sprintf_float_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            int n = sprintf(st->buf, "%.6f", st->vals[k]);
            sink_int = n;
            ops++;
        }
    }
    return ops;
}
static void cleanup_sprintf_float(void *state)
{
    sprintf_float_state *st = (sprintf_float_state *)state;
    free(st->vals);
    free(st->buf);
    free(st);
}

/* 16. snprintf mix */
typedef struct
{
    char *buf;
    int *ivals;
    double *dvals;
    size_t n;
} snprintf_state;
static void init_snprintf(void **state)
{
    snprintf_state *st = malloc(sizeof(*st));
    st->n = 128;
    st->ivals = malloc(st->n * sizeof(int));
    st->dvals = malloc(st->n * sizeof(double));
    for (size_t i = 0; i < st->n; i++)
    {
        st->ivals[i] = (int)i * 37;
        st->dvals[i] = i * 0.125 + 0.333;
    }
    st->buf = malloc(256);
    *state = st;
}
static size_t run_snprintf(void *state, size_t iters)
{
    snprintf_state *st = (snprintf_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            int n = snprintf(st->buf, 256, "idx=%zu iv=%d dv=%.4f hex=%x", k, st->ivals[k], st->dvals[k], st->ivals[k]);
            sink_int = n;
            ops++;
        }
    }
    return ops;
}
static void cleanup_snprintf(void *state)
{
    snprintf_state *st = (snprintf_state *)state;
    free(st->ivals);
    free(st->dvals);
    free(st->buf);
    free(st);
}

/* 17. strtod parse */
typedef struct
{
    char **nums;
    size_t n;
} strtod_state;
static void init_strtod(void **state)
{
    strtod_state *st = malloc(sizeof(*st));
    st->n = 256;
    st->nums = malloc(st->n * sizeof(char *));
    for (size_t i = 0; i < st->n; i++)
    {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%zu.%03zuE-%zu", i + 1, i % 1000, (i % 10) + 1);
        st->nums[i] = strdup(tmp);
    }
    *state = st;
}
static size_t run_strtod(void *state, size_t iters)
{
    strtod_state *st = (strtod_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            char *end;
            double v = strtod(st->nums[k], &end);
            sink_double = v;
            ops++;
        }
    }
    return ops;
}
static void cleanup_strtod(void *state)
{
    strtod_state *st = (strtod_state *)state;
    for (size_t i = 0; i < st->n; i++)
        free(st->nums[i]);
    free(st->nums);
    free(st);
}

/* 18. strtok parse */
typedef struct
{
    char *orig;
    size_t len;
} strtok_state;
static void init_strtok(void **state)
{
    strtok_state *st = malloc(sizeof(*st));
    const char *pattern = "alpha,beta,gamma,delta,epsilon,zeta,eta,theta,iota,kappa,lambda,mu,nu,xi,omicron,pi,rho,sigma,tau,upsilon,phi,chi,psi,omega";
    size_t plen = strlen(pattern);
    st->len = plen * 8 + 1;
    st->orig = malloc(st->len);
    st->orig[0] = '\0';
    for (int i = 0; i < 8; i++)
        strcat(st->orig, pattern);
    *state = st;
}
static size_t run_strtok(void *state, size_t iters)
{
    strtok_state *st = (strtok_state *)state;
    size_t ops = 0;
    char *buf = malloc(st->len);
    for (size_t i = 0; i < iters; i++)
    {
        strcpy(buf, st->orig);
        char *save;
        char *tok = strtok(buf, ",");
        while (tok)
        {
            sink_ptr = tok;
            ops++;
            tok = strtok(NULL, ",");
        }
    }
    free(buf);
    return ops;
}
static void cleanup_strtok(void *state)
{
    strtok_state *st = (strtok_state *)state;
    free(st->orig);
    free(st);
}

/* 19. regex match */
typedef struct
{
    regex_t rx;
    char **lines;
    size_t n;
} regex_state;
static void init_regex(void **state)
{
    regex_state *st = malloc(sizeof(*st));
    const char *pattern = "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,3}$";
    if (regcomp(&st->rx, pattern, REG_EXTENDED | REG_NOSUB) != 0)
    {
        fprintf(stderr, "regex compile failed\n");
        exit(1);
    }
    st->n = 128;
    st->lines = malloc(st->n * sizeof(char *));
    for (size_t i = 0; i < st->n; i++)
    {
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "user%zu_%zu@example%zu.com", i, i * i, i % 7);
        st->lines[i] = strdup(tmp);
    }
    *state = st;
}
static size_t run_regex(void *state, size_t iters)
{
    regex_state *st = (regex_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            int r = regexec(&st->rx, st->lines[k], 0, NULL, 0);
            sink_int = r;
            ops++;
        }
    }
    return ops;
}
static void cleanup_regex(void *state)
{
    regex_state *st = (regex_state *)state;
    regfree(&st->rx);
    for (size_t i = 0; i < st->n; i++)
        free(st->lines[i]);
    free(st->lines);
    free(st);
}

/* 20. atoi parse */
typedef struct
{
    char **nums;
    size_t n;
} atoi_state;
static void init_atoi(void **state)
{
    atoi_state *st = malloc(sizeof(*st));
    st->n = 512;
    st->nums = malloc(st->n * sizeof(char *));
    for (size_t i = 0; i < st->n; i++)
    {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%d", (int)((i * 37) % 1000000));
        st->nums[i] = strdup(tmp);
    }
    *state = st;
}
static size_t run_atoi(void *state, size_t iters)
{
    atoi_state *st = (atoi_state *)state;
    size_t ops = 0;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < st->n; k++)
        {
            int v = atoi(st->nums[k]);
            sink_int = v;
            ops++;
        }
    }
    return ops;
}
static void cleanup_atoi(void *state)
{
    atoi_state *st = (atoi_state *)state;
    for (size_t i = 0; i < st->n; i++)
        free(st->nums[i]);
    free(st->nums);
    free(st);
}

/* Benchmark registry */
static Benchmark benchmarks[] = {
    {"strlen", init_strlen, run_strlen, cleanup_free},
    {"strcmp", init_strcmp, run_strcmp, cleanup_strcmp},
    {"strcpy", init_strcpy, run_strcpy, cleanup_strcpy},
    {"strcat", init_strcat, run_strcat, cleanup_strcat},
    {"strchr", init_strchr, run_strchr, cleanup_strchr},
    {"memcmp", init_memcmp, run_memcmp, cleanup_memcmp},
    {"memcpy", init_memcpy, run_memcpy, cleanup_memcpy},
    {"memmove", init_memmove, run_memmove, cleanup_memmove},
    {"qsort_int", init_qsort, run_qsort, cleanup_qsort},
    {"bsearch_int", init_bsearch, run_bsearch, cleanup_bsearch},
    {"malloc_free_small", NULL, run_malloc_small, NULL},
    {"malloc_free_medium", NULL, run_malloc_medium, NULL},
    {"realloc_pattern", NULL, run_realloc_pattern, NULL},
    {"sprintf_int", init_sprintf_int, run_sprintf_int, cleanup_sprintf_int},
    {"sprintf_float", init_sprintf_float, run_sprintf_float, cleanup_sprintf_float},
    {"snprintf_mix", init_snprintf, run_snprintf, cleanup_snprintf},
    {"strtod_parse", init_strtod, run_strtod, cleanup_strtod},
    {"strtok_parse", init_strtok, run_strtok, cleanup_strtok},
    {"regex_match", init_regex, run_regex, cleanup_regex},
    {"atoi_parse", init_atoi, run_atoi, cleanup_atoi},
};

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-t target_ms]\n", prog);
}

int main(int argc, char **argv)
{
    uint64_t target_ms = 250;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc)
        {
            target_ms = (uint64_t)strtoull(argv[++i], NULL, 10);
        }
        else
        {
            usage(argv[0]);
            return 1;
        }
    }
    uint64_t target_ns = target_ms * 1000000ull;

    size_t count = sizeof(benchmarks) / sizeof(benchmarks[0]);
    printf("benchmark,operations,time_ns,ns_per_op,ops_per_sec\n");

    for (size_t i = 0; i < count; i++)
    {
        Benchmark *b = &benchmarks[i];
        void *state = NULL;
        if (b->init)
            b->init(&state);

        size_t iters = 1;
        uint64_t elapsed_ns = 0;
        size_t operations = 0;

        /* Calibration loop */
        while (1)
        {
            uint64_t start = now_ns();
            operations = b->run(state, iters);
            elapsed_ns = now_ns() - start;
            if (elapsed_ns >= target_ns || iters > (1ull << 30) || elapsed_ns > target_ns / 4)
                break;
            iters *= 2;
        }

        /* If too short, scale proportionally */
        if (elapsed_ns < target_ns / 2 && elapsed_ns > 0)
        {
            double scale = (double)target_ns / (double)elapsed_ns;
            size_t new_iters = (size_t)((double)iters * scale);
            if (new_iters > iters)
            {
                if (new_iters > iters * 8)
                    new_iters = iters * 8;
                uint64_t start = now_ns();
                operations = b->run(state, new_iters);
                elapsed_ns = now_ns() - start;
                iters = new_iters;
            }
        }

        double ns_per_op = operations ? (double)elapsed_ns / (double)operations : 0.0;

        printf("%s,%llu,%llu,%.2f\n",
               b->name,
               (unsigned long long)operations,
               (unsigned long long)elapsed_ns,
               ns_per_op);

        if (b->cleanup)
            b->cleanup(state);
    }
    return 0;
}