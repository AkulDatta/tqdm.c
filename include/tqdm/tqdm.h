#ifndef TQDM_H
#define TQDM_H

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include "tqdm/utils.h"

#ifndef SIZE_T_MAX
#define SIZE_T_MAX ((size_t)-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tqdm_s tqdm_t;
typedef struct tqdm_params_s tqdm_params_t;

/* Format dictionary structure */
typedef struct {
    size_t n;                 /* Current count */
    size_t total;             /* Total expected */
    double elapsed;           /* Elapsed time */
    double elapsed_s;         /* Elapsed seconds */
    char *desc;               /* Description */
    char *unit;               /* Unit string */
    double rate;              /* Current rate */
    char *rate_fmt;           /* Formatted rate */
    char *rate_noinv;         /* Rate without inverse */
    char *rate_noinv_fmt;     /* Formatted rate no inverse */
    char *rate_inv;           /* Inverse rate */
    char *rate_inv_fmt;       /* Formatted inverse rate */
    char *postfix;            /* Postfix string */
    double unit_divisor;      /* Unit divisor */
    double remaining;         /* Remaining time */
    double remaining_s;       /* Remaining seconds */
    double eta;               /* ETA timestamp */
    double percentage;        /* Completion percentage */
    char *n_fmt;              /* Formatted n */
    char *total_fmt;          /* Formatted total */
    int ncols;                /* Terminal columns */
    int nrows;                /* Terminal rows */
    char *l_bar;              /* Left bar part */
    char *bar;                /* Main bar part */
    char *r_bar;              /* Right bar part */
} tqdm_format_dict_t;

/* Postfix dictionary entry */
typedef struct postfix_entry_s {
    char *key;
    char *value;
    struct postfix_entry_s *next;
} postfix_entry_t;

/* Lock arguments structure for thread synchronization */
typedef struct {
    void *lock_ptr;
    int timeout_ms;
    bool noblock;
} lock_args_t;

#define TQDM_STRING_POOL_SIZE 16
#define TQDM_MAX_STRING_LEN 512

typedef struct {
    char strings[TQDM_STRING_POOL_SIZE][TQDM_MAX_STRING_LEN];
    bool in_use[TQDM_STRING_POOL_SIZE];
    size_t next_idx;
} tqdm_string_pool_t;

/* Cached values */
typedef struct {
    char *last_meter;
    char *last_postfix;
    char *cached_n_str;
    char *cached_total_str;
    char *cached_rate_str;
    char *cached_elapsed_str;
    char *cached_remaining_str;
    
    size_t last_n;
    size_t last_total;
    double last_rate;
    double last_elapsed;
    double last_remaining;
    int last_ncols;
    bool cache_valid;
} tqdm_cache_t;

/* Core parameters */
struct tqdm_params_s {
    char *desc;               /* Description prefix */
    size_t total;             /* Total expected iterations */
    bool leave;               /* Leave progress bar after completion */
    FILE *file;               /* Output file handle */
    int ncols;                /* Width of progress bar */
    float mininterval;        /* Minimum update interval */
    unsigned miniters;        /* Minimum update iterations */
    bool ascii;               /* Use ASCII instead of Unicode */
    bool disable;             /* Disable progress bar */
    char *unit;               /* Unit of measurement */
    bool unit_scale;          /* Auto-scale units */
    bool dynamic_ncols;       /* Dynamic column width */
    float smoothing;          /* Rate smoothing factor */
    char *bar_format;         /* Custom bar format */
    size_t initial;           /* Initial counter value */
    int position;             /* Line position for multiple bars */
    char *postfix;            /* Postfix string */
    float unit_divisor;       /* Unit divisor (1000 or 1024) */
    char *colour;             /* Progress bar colour */
    float delay;              /* Initial delay before showing */
};

/* tqdm data*/
struct tqdm_s {
    void *current;
    void *end;
    size_t element_size;
    size_t n;                 /* Current value */
    size_t count;             /* Total count */
    
    tqdm_params_t params;
    
    double start_time;
    double last_print_time;
    size_t last_print_count;
    bool closed;
    bool paused;
    double pause_start;
    double total_pause_time;
    
    double *rate_history;     /* Rate history for smoothing */
    size_t rate_history_size;
    size_t rate_history_idx;
    double cached_rate;
    double last_rate_calc_time;
    size_t last_rate_calc_n;
    
    int cached_terminal_width;
    double last_terminal_check;
    char display_buffer[1024]; /* Display buffer */
    
    pthread_mutex_t lock;
    
    bool iterator_mode;
    void *iterator_state;
    void *(*next_func)(void *state);
    bool (*has_next_func)(void *state);
    void (*destroy_func)(void *state);
};

typedef struct {
    int current;
    int total;
    int step;
} range_iterator_t;

/* Global lock */
extern pthread_mutex_t *tqdm_global_lock;

/* Core functions */
tqdm_t *tqdm_create(void *begin, void *end, size_t element_size);
tqdm_t *tqdm_create_with_total(void *begin, size_t total, size_t element_size);
tqdm_t *tqdm_create_with_params(void *begin, void *end, size_t element_size, tqdm_params_t *params);
tqdm_t *tqdm_create_iterator(void *iterator_state, 
                            void *(*next_func)(void *), 
                            bool (*has_next_func)(void *),
                            void (*destroy_func)(void *));
void tqdm_destroy(tqdm_t *tqdm);

/* Iterator */
bool tqdm_has_next(tqdm_t *tqdm);
void *tqdm_next(tqdm_t *tqdm);
tqdm_t *tqdm_iter(tqdm_t *tqdm);

/* Update */
void tqdm_update(tqdm_t *tqdm);
void tqdm_update_n(tqdm_t *tqdm, size_t n);
bool tqdm_update_to(tqdm_t *tqdm, size_t n);

/* tqdm functions */
void tqdm_close(tqdm_t *tqdm);
void tqdm_clear(tqdm_t *tqdm);
void tqdm_refresh(tqdm_t *tqdm);
void tqdm_unpause(tqdm_t *tqdm);
void tqdm_reset(tqdm_t *tqdm, size_t total);
void tqdm_set_description(tqdm_t *tqdm, const char *desc);
void tqdm_set_description_str(tqdm_t *tqdm, const char *desc, bool refresh);
void tqdm_set_postfix(tqdm_t *tqdm, postfix_entry_t *postfix);
void tqdm_set_postfix_str(tqdm_t *tqdm, const char *postfix, bool refresh);
void tqdm_write(const char *s, FILE *file, const char *end, bool nolock);
void tqdm_display(tqdm_t *tqdm, const char *msg, int pos);

/* Format functions */
tqdm_format_dict_t *tqdm_format_dict(tqdm_t *tqdm);
char *tqdm_format_sizeof(double num, const char *suffix, int divisor);
char *tqdm_format_interval(double t);
char *tqdm_format_num(double n);
char *tqdm_format_meter(size_t n, size_t total, double elapsed, 
                       int ncols, const char *prefix, bool ascii,
                       const char *unit, bool unit_scale, double rate,
                       const char *bar_format, const char *postfix,
                       int unit_divisor, size_t initial, const char *colour);

/* Global lock */
void tqdm_set_lock(pthread_mutex_t *lock);
pthread_mutex_t *tqdm_get_lock(void);

/* Context managers for external write mode and file wrapper */
typedef struct {
    tqdm_t *tqdm;
    FILE *original_file;
    bool lock_acquired;
} tqdm_external_write_context_t;

tqdm_external_write_context_t *tqdm_external_write_mode(FILE *file, bool nolock);
void tqdm_external_write_mode_exit(tqdm_external_write_context_t *ctx);

/* File wrapper context manager for file operations */
typedef struct {
    tqdm_t *tqdm;
    FILE *stream;
    const char *method;
    size_t (*read_func)(void *ptr, size_t size, size_t nmemb, FILE *stream);
    size_t (*write_func)(const void *ptr, size_t size, size_t nmemb, FILE *stream);
} tqdm_wrapattr_context_t;

tqdm_wrapattr_context_t *tqdm_wrapattr(FILE *stream, const char *method, 
                                      size_t total, bool bytes, tqdm_params_t *params);
void tqdm_wrapattr_exit(tqdm_wrapattr_context_t *ctx);

/* Monitor thread */
void tqdm_start_monitor(tqdm_t *tqdm);
void tqdm_stop_monitor(tqdm_t *tqdm);
void *tqdm_monitor_thread(void *arg);

/* Range */
range_iterator_t *range_create(int n);
range_iterator_t *range_create_with_bounds(int start, int end);
range_iterator_t *range_create_with_step(int start, int end, int step);
void range_destroy(range_iterator_t *range);

bool range_has_next(range_iterator_t *range);
int range_next(range_iterator_t *range);

/* Helper functions for tqdm parameters */
tqdm_params_t tqdm_default_params(void);
void tqdm_cleanup_params(tqdm_params_t *params);
void tqdm_update_dynamic_miniters(tqdm_t *tqdm);

/* Unicode block characters for progress bar */
extern const char *tqdm_unicode_blocks[];
extern const char *tqdm_ascii_blocks;

/* Convenience functions for a more similar API to Python tqdm */
range_iterator_t *trange(int n);
range_iterator_t *trange_with_bounds(int start, int end);
range_iterator_t *trange_with_step(int start, int end, int step);

/* Environment variables */
void tqdm_load_env_vars(tqdm_params_t *params);

/* Postfix dictionary */
postfix_entry_t *postfix_create(void);
void postfix_add(postfix_entry_t **head, const char *key, const char *value);
void postfix_add_int(postfix_entry_t **head, const char *key, int value);
void postfix_add_float(postfix_entry_t **head, const char *key, double value);
char *postfix_format(postfix_entry_t *head);
void postfix_destroy(postfix_entry_t *head);

/* Pandas integration stub */
void tqdm_pandas_register(tqdm_params_t *params);

/* Automatic progress tracking macros */
#define TQDM_FOR(type, var, start, end) \
    for (range_iterator_t *_r_##var = trange_with_bounds(start, end); \
         _r_##var; \
         range_destroy(_r_##var), _r_##var = NULL) \
    for (type var; _r_##var && range_has_next(_r_##var) && (var = range_next(_r_##var), 1); )

/* Range iteration with custom step */
#define TQDM_FOR_STEP(type, var, start, end, step) \
    for (range_iterator_t *_r_##var = trange_with_step(start, end, step); \
         _r_##var; \
         range_destroy(_r_##var), _r_##var = NULL) \
    for (type var; _r_##var && range_has_next(_r_##var) && (var = range_next(_r_##var), 1); )

/* Array iteration with progress tracking */
#define TQDM_FOR_ARRAY(type, var, array, size) \
    for (tqdm_t *_t_##var = tqdm_create(array, (array) + (size), sizeof(*(array))); \
         _t_##var; \
         tqdm_destroy(_t_##var), _t_##var = NULL) \
    for (type var; _t_##var && tqdm_has_next(_t_##var) && (var = (type)tqdm_next(_t_##var), 1); )

/* Manual progress bar */
#define TQDM_MANUAL(var, total) \
    for (tqdm_t *var = tqdm_create_with_total(NULL, total, 0); \
         var; tqdm_destroy(var), var = NULL)

/* Progress update for manual tracking*/
#define TQDM_UPDATE(pbar) tqdm_update(pbar)

#ifdef __cplusplus
}
#endif

#endif /* TQDM_H */
