

#define _GNU_SOURCE
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

#if defined(__GNUC__) || defined(__clang__)
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

static void
die(const char *msg)
{
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((cold, noinline))
#endif
static void
out_of_memory_error()
{
    die("out of memory");
}

static inline void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if unlikely (!p)
    {
        out_of_memory_error();
    }
    return p;
}

/* ---------------- Benchmarks ---------------- */

/* 1. strlen */
static void init_strlen(void **state)
{
    char *s = xmalloc(1025);
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
    strcmp_state *st = xmalloc(sizeof(*st));
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
    strcpy_state *st = xmalloc(sizeof(*st));
    st->src = xmalloc(2048);
    for (int i = 0; i < 2047; i++)
        st->src[i] = 'a' + (i % 26);
    st->src[2047] = '\0';
    st->dst = xmalloc(2048);
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
    strcat_state *st = xmalloc(sizeof(*st));
    st->piece = strdup("segment1234567890");
    st->buf = xmalloc(1024);
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
    strchr_state *st = xmalloc(sizeof(*st));
    st->s = xmalloc(4097);
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
    memcmp_state *st = xmalloc(sizeof(*st));
    st->len = 8192;
    st->a = xmalloc(st->len);
    st->b = xmalloc(st->len);
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
    memcpy_state *st = xmalloc(sizeof(*st));
    st->len = 16384;
    st->src = xmalloc(st->len);
    st->dst = xmalloc(st->len);
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
    memmove_state *st = xmalloc(sizeof(*st));
    st->len = 16384;
    st->buf = xmalloc(st->len + 64);
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
    qsort_state *st = xmalloc(sizeof(*st));
    st->n = 4096;
    st->orig = xmalloc(st->n * sizeof(int));
    st->work = xmalloc(st->n * sizeof(int));
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
    bsearch_state *st = xmalloc(sizeof(*st));
    st->n = 4096;
    st->k = 128;
    st->arr = xmalloc(st->n * sizeof(int));
    for (size_t i = 0; i < st->n; i++)
        st->arr[i] = (int)(i * 2);
    st->keys = xmalloc(st->k * sizeof(int));
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
            ptrs[k] = xmalloc(32);
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
            ptrs[k] = xmalloc(4096);
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
        char *p = xmalloc(sz);
        for (int r = 0; r < 64; r++)
        {
            sz = (sz < 1024) ? sz * 2 : 16;
            char *np = realloc(p, sz);
            if (!np)
            {
                /* realloc failed: keep original p for single free after loop */
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
    sprintf_int_state *st = xmalloc(sizeof(*st));
    st->n = 256;
    st->vals = xmalloc(st->n * sizeof(int));
    for (size_t i = 0; i < st->n; i++)
        st->vals[i] = (int)(i * i + 12345);
    st->buf = xmalloc(32);
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
    sprintf_float_state *st = xmalloc(sizeof(*st));
    st->n = 128;
    st->vals = xmalloc(st->n * sizeof(double));
    for (size_t i = 0; i < st->n; i++)
        st->vals[i] = (double)i / 3.14159;
    st->buf = xmalloc(64);
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
    snprintf_state *st = xmalloc(sizeof(*st));
    st->n = 128;
    st->ivals = xmalloc(st->n * sizeof(int));
    st->dvals = xmalloc(st->n * sizeof(double));
    for (size_t i = 0; i < st->n; i++)
    {
        st->ivals[i] = (int)i * 37;
        st->dvals[i] = i * 0.125 + 0.333;
    }
    st->buf = xmalloc(256);
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
    strtod_state *st = xmalloc(sizeof(*st));
    st->n = 256;
    st->nums = xmalloc(st->n * sizeof(char *));
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
    strtok_state *st = xmalloc(sizeof(*st));
    const char *pattern = "alpha,beta,gamma,delta,epsilon,zeta,eta,theta,iota,kappa,lambda,mu,nu,xi,omicron,pi,rho,sigma,tau,upsilon,phi,chi,psi,omega";
    size_t plen = strlen(pattern);
    st->len = plen * 8 + 1;
    st->orig = xmalloc(st->len);
    st->orig[0] = '\0';
    for (int i = 0; i < 8; i++)
        strcat(st->orig, pattern);
    *state = st;
}
static size_t run_strtok(void *state, size_t iters)
{
    strtok_state *st = (strtok_state *)state;
    size_t ops = 0;
    char *buf = xmalloc(st->len);
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
    regex_state *st = xmalloc(sizeof(*st));
    const char *pattern = "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,3}$";
    if (regcomp(&st->rx, pattern, REG_EXTENDED | REG_NOSUB) != 0)
    {
        fprintf(stderr, "regex compile failed\n");
        exit(1);
    }
    st->n = 128;
    st->lines = xmalloc(st->n * sizeof(char *));
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
    atoi_state *st = xmalloc(sizeof(*st));
    st->n = 512;
    st->nums = xmalloc(st->n * sizeof(char *));
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

/* 21. strstr */
typedef struct
{
    char *haystack;
    char **needles;
    size_t n;
} strstr_state;

static void init_strstr(void **state)
{
    strstr_state *st = xmalloc(sizeof(*st));
    size_t hlen = 65536;
    st->haystack = xmalloc(hlen + 1);
    /* Build haystack: repeating pattern segments with some markers */
    const char *segment = "lorem_ipsum_dolor_sit_amet_consectetur_";
    size_t seglen = strlen(segment);
    for (size_t i = 0; i < hlen; i++)
        st->haystack[i] = segment[i % seglen];
    st->haystack[hlen] = '\0';

    /* Insert a few distinctive substrings at known positions */
    const char *markers[] = {"ALPHA_token_X", "BETA_token_Y", "GAMMA_token_Z"};
    size_t mcount = sizeof(markers) / sizeof(markers[0]);
    for (size_t m = 0; m < mcount; m++)
    {
        size_t pos = (hlen / (mcount + 1)) * (m + 1);
        if (pos + strlen(markers[m]) < hlen)
            strcpy(st->haystack + pos, markers[m]);
        // memcpy(st->haystack + pos, markers[m], strlen(markers[m]));
    }

    st->n = 16;
    st->needles = xmalloc(st->n * sizeof(char *));
    /* Half existing (from markers or segment pieces), half non-existing */
    for (size_t i = 0; i < st->n; i++)
    {
        char tmp[64];
        if (i < 5)
            snprintf(tmp, sizeof(tmp), "%s", markers[i % mcount]);
        else if (i < 8)
            snprintf(tmp, sizeof(tmp), "ipsum_dolor_sit");
        else if (i < 11)
            snprintf(tmp, sizeof(tmp), "consectetur_lorem");
        else
            snprintf(tmp, sizeof(tmp), "no_such_substring_%zu", i);
        st->needles[i] = strdup(tmp);
    }
    *state = st;
}

static size_t run_strstr(void *state, size_t iters)
{
    strstr_state *st = (strstr_state *)state;
    size_t ops = 0;
    size_t n = st->n;
    for (size_t i = 0; i < iters; i++)
    {
        for (size_t k = 0; k < n; k++)
        {
            char *p = strstr(st->haystack, st->needles[(i + k) % n]);
            sink_ptr = p;
            ops++;
        }
    }
    return ops;
}

static void cleanup_strstr(void *state)
{
    strstr_state *st = (strstr_state *)state;
    for (size_t i = 0; i < st->n; i++)
        free(st->needles[i]);
    free(st->needles);
    free(st->haystack);
    free(st);
}

#include <stdarg.h>

/* 22. memset */
typedef struct
{
    unsigned char *area;
    size_t len;
} bench_memset_state;
static void init_memset_bench(void **st)
{
    bench_memset_state *s = xmalloc(sizeof *s);
    s->len = 1 << 15;
    s->area = xmalloc(s->len);
    *st = s;
}
static size_t run_memset_bench(void *st, size_t loop)
{
    bench_memset_state *s = (bench_memset_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        int v = (int)(iter & 0xFF);
        memset(s->area, v, s->len);
        sink_int = v;
        ++count;
    }
    return count;
}
static void cleanup_memset_bench(void *st)
{
    bench_memset_state *s = (bench_memset_state *)st;
    free(s->area);
    free(s);
}

/* 23. memchr / memrchr (GNU) */
typedef struct
{
    unsigned char *blk;
    size_t len;
} bench_memchr_state;
static void init_memchr_bench(void **st)
{
    bench_memchr_state *s = xmalloc(sizeof *s);
    s->len = 1 << 14;
    s->blk = xmalloc(s->len);
    for (size_t i = 0; i < s->len; i++)
        s->blk[i] = (unsigned char)(i * 17);
    *st = s;
}
static size_t run_memchr_bench(void *st, size_t loop)
{
    bench_memchr_state *s = (bench_memchr_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        unsigned char needle = (unsigned char)(iter & 0xFF);
        void *p = memchr(s->blk, needle, s->len);
        sink_ptr = p;
        ++count;
    }
    return count;
}
static size_t run_memrchr_bench(void *st, size_t loop)
{
    bench_memchr_state *s = (bench_memchr_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        unsigned char needle = (unsigned char)((iter * 3) & 0xFF);
        void *p = memrchr(s->blk, needle, s->len);
        sink_ptr = p;
        ++count;
    }
    return count;
}
static void cleanup_memchr_bench(void *st)
{
    bench_memchr_state *s = (bench_memchr_state *)st;
    free(s->blk);
    free(s);
}

/* 24. strnlen */
typedef struct
{
    char *txt;
    size_t cap;
} bench_strnlen_state;
static void init_strnlen_bench(void **st)
{
    bench_strnlen_state *s = xmalloc(sizeof *s);
    s->cap = 4096;
    s->txt = xmalloc(s->cap);
    for (size_t i = 0; i < s->cap - 1; i++)
        s->txt[i] = (i % 97) ? 'a' + (i % 26) : '\0';
    s->txt[s->cap - 1] = '\0';
    *st = s;
}
static size_t run_strnlen_bench(void *st, size_t loop)
{
    bench_strnlen_state *s = (bench_strnlen_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        size_t lim = 16 + (iter % s->cap);
        size_t L = strnlen(s->txt, lim);
        sink_size = L;
        ++count;
    }
    return count;
}
static void cleanup_strnlen_bench(void *st)
{
    bench_strnlen_state *s = (bench_strnlen_state *)st;
    free(s->txt);
    free(s);
}

/* 25. strncmp */
typedef struct
{
    char *a;
    char *b;
    size_t len;
} bench_strncmp_state;
static void init_strncmp_bench(void **st)
{
    bench_strncmp_state *s = xmalloc(sizeof *s);
    s->len = 2048;
    s->a = xmalloc(s->len + 1);
    s->b = xmalloc(s->len + 1);
    for (size_t i = 0; i < s->len; i++)
    {
        char c = 'a' + (i % 26);
        s->a[i] = c;
        s->b[i] = c;
    }
    s->a[s->len] = '\0';
    s->b[s->len] = '\0';
    s->b[s->len / 2] = 'Z';
    *st = s;
}
static size_t run_strncmp_bench(void *st, size_t loop)
{
    bench_strncmp_state *s = (bench_strncmp_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        size_t n = 8 + ((iter * 13) % (s->len));
        int r = strncmp(s->a, s->b, n);
        sink_int = r;
        ++count;
    }
    return count;
}
static void cleanup_strncmp_bench(void *st)
{
    bench_strncmp_state *s = (bench_strncmp_state *)st;
    free(s->a);
    free(s->b);
    free(s);
}

/* 26. strncpy */
typedef struct
{
    char *src;
    char *dst;
    size_t len;
} bench_strncpy_state;
static void init_strncpy_bench(void **st)
{
    bench_strncpy_state *s = xmalloc(sizeof *s);
    s->len = 4096;
    s->src = xmalloc(s->len + 1);
    s->dst = xmalloc(s->len + 16);
    for (size_t i = 0; i < s->len; i++)
        s->src[i] = 'A' + (i % 26);
    s->src[s->len] = '\0';
    *st = s;
}
static size_t run_strncpy_bench(void *st, size_t loop)
{
    bench_strncpy_state *s = (bench_strncpy_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        size_t n = 32 + (iter % s->len);
        size_t dst_cap = s->len + 16; /* allocated size of dst */
        if (n > dst_cap)
            n = dst_cap; /* clamp to avoid overflow & zero padding beyond allocation */
        char *r = strncpy(s->dst, s->src, n);
        sink_ptr = r;
        ++count;
    }
    return count;
}
static void cleanup_strncpy_bench(void *st)
{
    bench_strncpy_state *s = (bench_strncpy_state *)st;
    free(s->src);
    free(s->dst);
    free(s);
}

/* 27. strncat */
typedef struct
{
    char *dst;
    char *piece;
    size_t cap;
} bench_strncat_state;
static void init_strncat_bench(void **st)
{
    bench_strncat_state *s = xmalloc(sizeof *s);
    s->cap = 8192;
    s->dst = xmalloc(s->cap);
    s->piece = strdup("segment_data_block_");
    *st = s;
}
static size_t run_strncat_bench(void *st, size_t loop)
{
    bench_strncat_state *s = (bench_strncat_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        s->dst[0] = '\0';
        for (int r = 0; r < 16; r++)
        {
            strncat(s->dst, s->piece, 8 + (iter & 7));
            ++count;
        }
        sink_size = strlen(s->dst);
    }
    return count;
}
static void cleanup_strncat_bench(void *st)
{
    bench_strncat_state *s = (bench_strncat_state *)st;
    free(s->dst);
    free(s->piece);
    free(s);
}

/* 28. strrchr */
typedef struct
{
    char *text;
} bench_strrchr_state;
static void init_strrchr_bench(void **st)
{
    bench_strrchr_state *s = xmalloc(sizeof *s);
    size_t L = 10000;
    s->text = xmalloc(L + 1);
    for (size_t i = 0; i < L; i++)
    {
        s->text[i] = (i % 101) == 0 ? 'X' : 'a' + (i % 26);
    }
    s->text[L] = '\0';
    *st = s;
}
static size_t run_strrchr_bench(void *st, size_t loop)
{
    bench_strrchr_state *s = (bench_strrchr_state *)st;
    size_t count = 0;
    size_t len = strlen(s->text);
    for (size_t iter = 0; iter < loop; ++iter)
    {
        /* Mutate one character so the location of the last 'X' changes,
           preventing the optimizer from treating strrchr() as a pure/constant call. */
        size_t idx = (iter * 131u) % len;
        char prev = s->text[idx];
        s->text[idx] = (prev == 'X') ? ('a' + (char)(iter % 26)) : 'X';

        char *p = strrchr(s->text, 'X');
        /* Use the offset (accumulated into a volatile) so result is needed. */
        if (p)
            sink_size += (size_t)(p - s->text);
        ++count;
    }
    return count;
}
static void cleanup_strrchr_bench(void *st)
{
    bench_strrchr_state *s = (bench_strrchr_state *)st;
    free(s->text);
    free(s);
}

/* 29. strtok_r */
typedef struct
{
    char *orig;
    size_t len;
} bench_strtok_r_state;
static void init_strtok_r_bench(void **st)
{
    bench_strtok_r_state *s = xmalloc(sizeof *s);
    const char *src = "aa,bb,cc,dd,ee,ff,gg,hh,ii,jj,kk,ll,mm,nn,oo,pp,qq";
    size_t L = strlen(src);
    s->len = L * 16 + 1;
    s->orig = xmalloc(s->len);
    s->orig[0] = '\0';
    for (int k = 0; k < 16; k++)
        strcat(s->orig, src);
    *st = s;
}
static size_t run_strtok_r_bench(void *st, size_t loop)
{
    bench_strtok_r_state *s = (bench_strtok_r_state *)st;
    char *buf = xmalloc(s->len);
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        strcpy(buf, s->orig);
        char *ctx = NULL;
        char *tok = strtok_r(buf, ",", &ctx);
        while (tok)
        {
            sink_ptr = tok;
            ++count;
            tok = strtok_r(NULL, ",", &ctx);
        }
    }
    free(buf);
    return count;
}
static void cleanup_strtok_r_bench(void *st)
{
    bench_strtok_r_state *s = (bench_strtok_r_state *)st;
    free(s->orig);
    free(s);
}

/* 30. FILE I/O throughput (fwrite/fread/fseek) */
typedef struct
{
    unsigned char *data;
    size_t len;
} bench_file_io_state;
static void init_file_io_bench(void **st)
{
    bench_file_io_state *s = xmalloc(sizeof *s);
    s->len = 1 << 20;
    s->data = xmalloc(s->len);
    for (size_t i = 0; i < s->len; i++)
        s->data[i] = (unsigned char)(i * 31);
    *st = s;
}
static size_t run_fwrite_fread_bench(void *st, size_t loop)
{
    bench_file_io_state *s = (bench_file_io_state *)st;
    size_t count = 0;
    unsigned char *tmp = xmalloc(s->len);
    for (size_t iter = 0; iter < loop; ++iter)
    {
        FILE *fp = tmpfile();
        if (!fp)
            break;
        size_t w = fwrite(s->data, 1, s->len, fp);
        fseek(fp, 0, SEEK_SET);
        size_t r = fread(tmp, 1, s->len, fp);
        sink_size = w + r;
        fclose(fp);
        ++count;
    }
    free(tmp);
    return count;
}
static void cleanup_file_io_bench(void *st)
{
    bench_file_io_state *s = (bench_file_io_state *)st;
    free(s->data);
    free(s);
}

/* 31. fgets / getline */
typedef struct
{
    char *big;
    size_t sz;
} bench_line_in_state;
static void init_line_in_bench(void **st)
{
    bench_line_in_state *s = xmalloc(sizeof *s);
    size_t lines = 5000;
    s->sz = lines * 32 + 1;
    s->big = xmalloc(s->sz);
    char *p = s->big;
    for (size_t i = 0; i < lines; i++)
    {
        int n = sprintf(p, "line_%zu value=%zu\n", i, i * i);
        p += n;
    }
    *p = '\0';
    *st = s;
}
static size_t run_fgets_bench(void *st, size_t loop)
{
    bench_line_in_state *s = (bench_line_in_state *)st;
    size_t count = 0;
    char buf[128];
    for (size_t iter = 0; iter < loop; ++iter)
    {
        FILE *fp = fmemopen(s->big, s->sz, "r");
        if (!fp)
            break;
        while (fgets(buf, sizeof buf, fp))
        {
            sink_int += (int)buf[0];
            ++count;
        }
        fclose(fp);
    }
    return count;
}
static size_t run_getline_bench(void *st, size_t loop)
{
    bench_line_in_state *s = (bench_line_in_state *)st;
    size_t count = 0;

    for (size_t iter = 0; iter < loop; ++iter)
    {
        FILE *fp = fmemopen(s->big, s->sz, "r");
        if (!fp)
            break;
        char *line = NULL;
        size_t n = 0;
        while (getline(&line, &n, fp) > 0)
        {
            sink_int += (int)line[0];
            ++count;
        }
        free(line);
        fclose(fp);
    }
    return count;
}
static void cleanup_line_in_bench(void *st)
{
    bench_line_in_state *s = (bench_line_in_state *)st;
    free(s->big);
    free(s);
}

/* 32. vprintf / vsnprintf */
static int helper_vsnp(char *dst, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rv = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return rv;
}
typedef struct
{
    char *buf;
} bench_vprintf_state;
static void init_vprintf_bench(void **st)
{
    bench_vprintf_state *s = xmalloc(sizeof *s);
    s->buf = xmalloc(512);
    *st = s;
}
static size_t run_vsnprintf_bench(void *st, size_t loop)
{
    bench_vprintf_state *s = (bench_vprintf_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        int r = helper_vsnp(s->buf, 512, "val=%d hex=%x str=%s dbl=%.3f",
                            (int)iter, (unsigned)(iter * 17), "token", (double)iter / 3.0);
        sink_int = r;
        ++count;
    }
    return count;
}
static size_t run_vprintf_bench(void *st, size_t loop)
{
    bench_vprintf_state *s = (bench_vprintf_state *)st;
    size_t count = 0;
    for (size_t iter = 0; iter < loop; ++iter)
    {
        int r = helper_vsnp(s->buf, 512, "A:%d B:%u C:%ld D:%0.2f",
                            (int)iter, (unsigned)iter, (long)(iter * iter), (double)iter / 7.0);
        sink_int = r;
        ++count;
    }
    return count;
}
static void cleanup_vprintf_bench(void *st)
{
    bench_vprintf_state *s = (bench_vprintf_state *)st;
    free(s->buf);
    free(s);
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
    {"strstr_search", init_strstr, run_strstr, cleanup_strstr},
    {"strnlen", init_strnlen_bench, run_strnlen_bench, cleanup_strnlen_bench},
    {"strncmp", init_strncmp_bench, run_strncmp_bench, cleanup_strncmp_bench},
    {"strncpy", init_strncpy_bench, run_strncpy_bench, cleanup_strncpy_bench},
    {"strncat", init_strncat_bench, run_strncat_bench, cleanup_strncat_bench},
    {"strrchr", init_strrchr_bench, run_strrchr_bench, cleanup_strrchr_bench},
    {"strtok_r_parse", init_strtok_r_bench, run_strtok_r_bench, cleanup_strtok_r_bench},
    {"file_io_rw", init_file_io_bench, run_fwrite_fread_bench, cleanup_file_io_bench},
    {"fgets_read", init_line_in_bench, run_fgets_bench, cleanup_line_in_bench},
    {"getline_read", init_line_in_bench, run_getline_bench, cleanup_line_in_bench},
    {"vsnprintf_mix", init_vprintf_bench, run_vsnprintf_bench, cleanup_vprintf_bench},
    {"vprintf_mix", init_vprintf_bench, run_vprintf_bench, cleanup_vprintf_bench},
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