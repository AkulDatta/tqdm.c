#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "tqdm/tqdm.h"

/* =============================
 * Globals / constants
 * ============================= */
pthread_mutex_t *tqdm_global_lock = NULL;
static pthread_mutex_t default_lock = PTHREAD_MUTEX_INITIALIZER;

/* Unicode block characters for the progress bar */
const char *tqdm_unicode_blocks[] = {" ", "▏", "▎", "▍", "▌",
                                     "▋", "▊", "▉", "█"};
const char *tqdm_ascii_blocks = " 123456789#";

#define TQDM_ENV_PREFIX "TQDM_"

/* =============================
 * Tiny utility helpers
 * ============================= */
static double current_time_seconds(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static int get_terminal_width(void) {
  struct winsize w;
  return (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) ? w.ws_col : 80;
}

static int get_terminal_height(void) {
  struct winsize w;
  return (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) ? w.ws_row : 24;
}

/* Print progress helper (forward decl) */
static void tqdm_print_progress(tqdm_t *tqdm);

/* Default parameters */
tqdm_params_t tqdm_default_params(void) {
  tqdm_params_t params;
  memset(&params, 0, sizeof(params));

  params.desc = NULL;
  params.total = 0;
  params.leave = true;
  params.file = stderr;
  params.ncols = -1;
  params.mininterval = 0.1f;
  params.miniters = 0;
  params.ascii = false;
  params.disable = false;
  params.unit = strdup("it");
  params.unit_scale = false;
  params.dynamic_ncols = false;
  params.smoothing = 0.3f;
  params.bar_format = NULL;
  params.initial = 0;
  params.position = -1;
  params.postfix = NULL;
  params.unit_divisor = 1000.0f;
  params.colour = NULL;
  params.delay = 0.0f;

  return params;
}

/* Cleanup */
void tqdm_cleanup_params(tqdm_params_t *params) {
  if (!params)
    return;

  free(params->desc);
  free(params->unit);
  free(params->bar_format);
  free(params->colour);
  free(params->postfix);

  memset(params, 0, sizeof(*params));
}

/* Environment variable loading */
void tqdm_load_env_vars(tqdm_params_t *params) {
  char *env_val;

  if ((env_val = getenv("TQDM_MININTERVAL")) != NULL) {
    params->mininterval = atof(env_val);
  }
  if ((env_val = getenv("TQDM_MINITERS")) != NULL) {
    params->miniters = atoi(env_val);
  }
  if ((env_val = getenv("TQDM_ASCII")) != NULL) {
    params->ascii =
        (strcasecmp(env_val, "true") == 0 || strcmp(env_val, "1") == 0);
  }
  if ((env_val = getenv("TQDM_DISABLE")) != NULL) {
    params->disable =
        (strcasecmp(env_val, "true") == 0 || strcmp(env_val, "1") == 0);
  }
  if ((env_val = getenv("TQDM_UNIT")) != NULL) {
    if (params->unit)
      free(params->unit);
    params->unit = strdup(env_val);
  }
  if ((env_val = getenv("TQDM_UNIT_SCALE")) != NULL) {
    params->unit_scale =
        (strcasecmp(env_val, "true") == 0 || strcmp(env_val, "1") == 0);
  }
  if ((env_val = getenv("TQDM_DYNAMIC_NCOLS")) != NULL) {
    params->dynamic_ncols =
        (strcasecmp(env_val, "true") == 0 || strcmp(env_val, "1") == 0);
  }
  if ((env_val = getenv("TQDM_SMOOTHING")) != NULL) {
    params->smoothing = atof(env_val);
  }
  if ((env_val = getenv("TQDM_NCOLS")) != NULL) {
    params->ncols = atoi(env_val);
  }
  if ((env_val = getenv("TQDM_COLOUR")) != NULL) {
    if (params->colour)
      free(params->colour);
    params->colour = strdup(env_val);
  }
  if ((env_val = getenv("TQDM_DELAY")) != NULL) {
    params->delay = atof(env_val);
  }
}

/* =============================
 * Thread-safety helpers
 * ============================= */
void tqdm_set_lock(pthread_mutex_t *lock) { tqdm_global_lock = lock; }

pthread_mutex_t *tqdm_get_lock(void) {
  if (tqdm_global_lock == NULL) {
    tqdm_global_lock = &default_lock;
  }
  return tqdm_global_lock;
}

/* =============================
 * Monitor stubs (compatibility only)
 * ============================= */
void tqdm_start_monitor(tqdm_t *tqdm) { (void)tqdm; }

void tqdm_stop_monitor(tqdm_t *tqdm) { (void)tqdm; }

void *tqdm_monitor_thread(void *arg) {
  (void)arg;
  return NULL;
}

/* Format dictionary */
tqdm_format_dict_t *tqdm_format_dict(tqdm_t *tqdm) {
  static tqdm_format_dict_t simple_dict;
  memset(&simple_dict, 0, sizeof(simple_dict));

  double current_time = current_time_seconds();
  double elapsed = current_time - tqdm->start_time - tqdm->total_pause_time;

  simple_dict.n = tqdm->n;
  simple_dict.total = tqdm->params.total;
  simple_dict.elapsed = elapsed;
  simple_dict.elapsed_s = elapsed;
  simple_dict.rate = elapsed > 0 ? (double)tqdm->n / elapsed : 0.0;
  simple_dict.percentage =
      tqdm->params.total > 0 ? (100.0 * tqdm->n) / tqdm->params.total : 0.0;
  simple_dict.ncols = get_terminal_width();
  simple_dict.nrows = get_terminal_height();
  simple_dict.unit_divisor = tqdm->params.unit_divisor;

  return &simple_dict;
}

/* Dynamic miniters */
void tqdm_update_dynamic_miniters(tqdm_t *tqdm) {
  double current_time = current_time_seconds();
  double time_diff = current_time - tqdm->last_print_time;

  if (time_diff > 0 && tqdm->params.miniters == 0) {
    size_t count_diff = tqdm->n - tqdm->last_print_count;
    if (count_diff > 0 && time_diff < tqdm->params.mininterval) {
      tqdm->params.miniters = (unsigned)(count_diff * 2);
    }
  }
}

/* =============================
 * Formatting helpers
 * ============================= */
static char *fast_format_int(long long num, char *buffer, size_t size) {
  if (num == 0) {
    strncpy(buffer, "0", size);
    return buffer;
  }

  char temp[32];
  char *p = temp + sizeof(temp) - 1;
  *p = '\0';

  bool negative = num < 0;
  if (negative)
    num = -num;

  while (num > 0) {
    *--p = '0' + (num % 10);
    num /= 10;
  }

  if (negative)
    *--p = '-';

  strncpy(buffer, p, size);
  return buffer;
}

char *tqdm_format_sizeof(double num, const char *suffix, int divisor) {
  static const char *prefixes[] = {"",  "k", "M", "G", "T",
                                   "P", "E", "Z", "Y"};
  int prefix_idx = 0;

  if (divisor == 1024) {
    while (num >= 1024.0 && prefix_idx < 8) {
      num /= 1024.0;
      prefix_idx++;
    }
  } else {
    while (num >= divisor && prefix_idx < 8) {
      num /= divisor;
      prefix_idx++;
    }
  }

  char *result = malloc(64);
  const char *suffix_str = suffix ? suffix : "";

  if (num == (double)(long long)num && num < 1000000) {
    snprintf(result, 64, "%lld%s%s", (long long)num, prefixes[prefix_idx],
             suffix_str);
  } else if (num >= 100 || prefix_idx == 0) {
    snprintf(result, 64, "%.0f%s%s", num, prefixes[prefix_idx], suffix_str);
  } else if (num >= 10) {
    snprintf(result, 64, "%.1f%s%s", num, prefixes[prefix_idx], suffix_str);
  } else {
    snprintf(result, 64, "%.2f%s%s", num, prefixes[prefix_idx], suffix_str);
  }

  return result;
}

char *tqdm_format_interval(double t) {
  if (t < 0 || t > 86400 * 365) {
    char *result = malloc(2);
    strcpy(result, "?");
    return result;
  }

  int total_seconds = (int)t;
  int hours = total_seconds / 3600;
  int minutes = (total_seconds % 3600) / 60;
  int seconds = total_seconds % 60;

  if (hours > 99999)
    hours = 99999;
  if (minutes > 59)
    minutes = 59;
  if (seconds > 59)
    seconds = 59;

  char *result = malloc(16);

  if (hours > 0) {
    snprintf(result, 16, "%02d:%02d:%02d", hours, minutes, seconds);
  } else {
    snprintf(result, 16, "%02d:%02d", minutes, seconds);
  }

  return result;
}

char *tqdm_format_num(double n) {
  char *result = malloc(32);

  /* Human‐readable suffix formatting: k (thousand), m (million), b (billion),
   * t (trillion). For numbers beyond 1e15 we fall back to scientific notation
   */
  double absn = fabs(n);
  const char *suffix = "";
  double scaled = n;
  if (absn >= 1e12 && absn < 1e15) {
    suffix = "t";
    scaled = n / 1e12;
  } else if (absn >= 1e9) {
    suffix = "b";
    scaled = n / 1e9;
  } else if (absn >= 1e6) {
    suffix = "m";
    scaled = n / 1e6;
  } else if (absn >= 1e3) {
    suffix = "k";
    scaled = n / 1e3;
  }

  if (suffix[0] != '\0') {
    if (fabs(scaled) >= 100) {
      snprintf(result, 32, "%.0f%s", scaled, suffix);
    } else if (fabs(scaled) >= 10) {
      snprintf(result, 32, "%.1f%s", scaled, suffix);
    } else {
      snprintf(result, 32, "%.2f%s", scaled, suffix);
    }
  } else if (absn < 1000 && n == (double)(long long)n) {
    fast_format_int((long long)n, result, 32);
  } else if (absn < 1e15) {
    snprintf(result, 32, "%.0f", n);
  } else {
    snprintf(result, 32, "%.3g", n);
  }

  return result;
}

char *tqdm_format_meter(size_t n, size_t total, double elapsed, int ncols,
                        const char *prefix, bool ascii, const char *unit,
                        bool unit_scale, double rate, const char *bar_format,
                        const char *postfix, int unit_divisor, size_t initial,
                        const char *colour) {
  (void)initial;
  (void)colour;

  char *result = NULL;

  if (bar_format && strlen(bar_format) > 0) {
    result = malloc(256);
    snprintf(result, 256, "%s: %zu/%zu [%.1fs, %.1fit/s] %s",
             prefix ? prefix : "", n, total, elapsed, rate,
             postfix ? postfix : "");
    return result;
  }

  double percentage = total > 0 ? (100.0 * n) / total : 0.0;
  if (percentage > 100.0)
    percentage = 100.0;

  char *remaining_str;
  if (total > 0 && n > 0 && rate > 0 && n < total) {
    double remaining = (total - n) / rate;
    remaining_str = tqdm_format_interval(remaining);
  } else {
    remaining_str = malloc(2);
    strcpy(remaining_str, "?");
  }

  char *elapsed_str = tqdm_format_interval(elapsed);

  char *n_str, *total_str, *rate_str;
  if (unit_scale) {
    n_str = tqdm_format_sizeof((double)n, unit, unit_divisor);
    if (total > 0) {
      total_str = tqdm_format_sizeof((double)total, unit, unit_divisor);
    } else {
      total_str = malloc(2);
      strcpy(total_str, "?");
    }
    if (rate <= 0) {
      rate_str = malloc(2);
      strcpy(rate_str, "?");
    } else {
      rate_str = tqdm_format_sizeof(rate, unit, unit_divisor);
    }
  } else {
    n_str = tqdm_format_num((double)n);
    if (total > 0) {
      total_str = tqdm_format_num((double)total);
    } else {
      total_str = malloc(2);
      strcpy(total_str, "?");
    }
    if (rate <= 0) {
      rate_str = malloc(2);
      strcpy(rate_str, "?");
    } else {
      rate_str = tqdm_format_num(rate);
    }
  }

  int estimated_fixed = 50;
  if (prefix)
    estimated_fixed += strlen(prefix);
  if (postfix)
    estimated_fixed += strlen(postfix);

  int bar_width = (ncols > estimated_fixed) ? (ncols - estimated_fixed) : 10;
  if (bar_width < 1)
    bar_width = 1;
  if (bar_width > 100)
    bar_width = 100;

  char *bar = malloc(bar_width * 4 + 1);

  if (ascii) {
    int filled = total > 0 ? (int)((double)bar_width * n / total) : 0;
    if (filled > bar_width)
      filled = bar_width;

    memset(bar, '#', filled);
    memset(bar + filled, ' ', bar_width - filled);
    bar[bar_width] = '\0';
  } else {
    bar[0] = '\0';

    if (total > 0 && n > 0) {
      int progress_fixed = (int)((n * 8 * bar_width) / total);
      int filled_full = progress_fixed / 8;
      int partial = progress_fixed % 8;

      if (filled_full > bar_width) {
        filled_full = bar_width;
        partial = 0;
      }

      char *bar_ptr = bar;

      for (int i = 0; i < filled_full && i < bar_width; i++) {
        strcpy(bar_ptr, tqdm_unicode_blocks[8]);
        bar_ptr += strlen(tqdm_unicode_blocks[8]);
      }

      if (filled_full < bar_width && partial > 0) {
        strcpy(bar_ptr, tqdm_unicode_blocks[partial]);
        bar_ptr += strlen(tqdm_unicode_blocks[partial]);
        filled_full++;
      }

      for (int i = filled_full; i < bar_width; i++) {
        strcpy(bar_ptr, tqdm_unicode_blocks[0]);
        bar_ptr += strlen(tqdm_unicode_blocks[0]);
      }
    } else {
      char *bar_ptr = bar;
      for (int i = 0; i < bar_width; i++) {
        strcpy(bar_ptr, tqdm_unicode_blocks[0]);
        bar_ptr += strlen(tqdm_unicode_blocks[0]);
      }
    }
  }

  result = malloc(1024);

  const char *desc_part = prefix && strlen(prefix) > 0 ? prefix : "";
  const char *desc_sep = prefix && strlen(prefix) > 0 ? ": " : "";
  const char *unit_str = unit ? unit : "it";
  const char *rate_unit_suffix = unit_scale ? "" : unit_str;
  const char *postfix_sep = postfix && strlen(postfix) > 0 ? " " : "";
  const char *postfix_str = postfix ? postfix : "";

  snprintf(result, 1024, "%s%s%3.0f%%|%s| %s/%s [%s<%s, %s%s/s]%s%s",
           desc_part, desc_sep, percentage, bar, n_str, total_str,
           elapsed_str, remaining_str, rate_str, rate_unit_suffix,
           postfix_sep, postfix_str);

  free(bar);
  free(remaining_str);
  free(elapsed_str);
  free(n_str);
  free(total_str);
  free(rate_str);

  return result;
}

/* Postfix functions */
void tqdm_set_postfix(tqdm_t *tqdm, postfix_entry_t *postfix) {
  if (tqdm->params.postfix) {
    free(tqdm->params.postfix);
    tqdm->params.postfix = NULL;
  }

  if (postfix) {
    tqdm->params.postfix = postfix_format(postfix);
  }
}

/* Postfix dictionary */
postfix_entry_t *postfix_create(void) { return NULL; }

void postfix_add(postfix_entry_t **head, const char *key, const char *value) {
  if (!head || !key || !value)
    return;

  postfix_entry_t *entry = malloc(sizeof(postfix_entry_t));
  if (!entry)
    return;

  entry->key = strdup(key);
  entry->value = strdup(value);

  if (!entry->key || !entry->value) {
    free(entry->key);
    free(entry->value);
    free(entry);
    return;
  }

  entry->next = *head;
  *head = entry;
}

void postfix_add_int(postfix_entry_t **head, const char *key, int value) {
  char *value_str;
  asprintf(&value_str, "%d", value);
  postfix_add(head, key, value_str);
  free(value_str);
}

void postfix_add_float(postfix_entry_t **head, const char *key,
                       double value) {
  char *value_str = NULL;
  int len = snprintf(NULL, 0, "%.3g", value);
  if (len >= 0) {
    value_str = malloc(len + 1);
    if (value_str) {
      snprintf(value_str, len + 1, "%.3g", value);
      postfix_add(head, key, value_str);
      free(value_str);
    }
  }
}

char *postfix_format(postfix_entry_t *head) {
  if (!head) {
    char *result = malloc(1);
    result[0] = '\0';
    return result;
  }

  char *result = malloc(512);
  result[0] = '\0';

  postfix_entry_t *curr = head;
  bool first = true;

  while (curr) {
    if (!first) {
      strcat(result, ", ");
    }
    strcat(result, curr->key);
    strcat(result, "=");
    strcat(result, curr->value);
    first = false;
    curr = curr->next;
  }

  return result;
}

void postfix_destroy(postfix_entry_t *head) {
  while (head) {
    postfix_entry_t *next = head->next;
    free(head->key);
    free(head->value);
    free(head);
    head = next;
  }
}

/* Creation functions */
tqdm_t *tqdm_create(void *begin, void *end, size_t element_size) {
  tqdm_params_t params = tqdm_default_params();
  tqdm_t *result = tqdm_create_with_params(begin, end, element_size, &params);
  tqdm_cleanup_params(&params);
  return result;
}

tqdm_t *tqdm_create_with_total(void *begin, size_t total,
                               size_t element_size) {
  tqdm_params_t params = tqdm_default_params();
  params.total = total;
  tqdm_t *result =
      tqdm_create_with_params(begin, NULL, element_size, &params);
  tqdm_cleanup_params(&params);
  return result;
}

tqdm_t *tqdm_create_with_params(void *begin, void *end, size_t element_size,
                                tqdm_params_t *user_params) {
  tqdm_t *tqdm = calloc(1, sizeof(tqdm_t));
  if (!tqdm)
    return NULL;

  tqdm->params = *user_params;

  if (user_params->desc) {
    tqdm->params.desc = strdup(user_params->desc);
    if (!tqdm->params.desc)
      goto cleanup_and_fail;
  } else {
    tqdm->params.desc = NULL;
  }

  if (user_params->unit) {
    tqdm->params.unit = strdup(user_params->unit);
    if (!tqdm->params.unit)
      goto cleanup_and_fail;
  } else {
    tqdm->params.unit = NULL;
  }

  if (user_params->ascii) {
    tqdm->params.ascii = user_params->ascii; /* Create a boolean copy */
  } else {
    tqdm->params.ascii = false;
  }

  if (user_params->bar_format) {
    tqdm->params.bar_format = strdup(user_params->bar_format);
    if (!tqdm->params.bar_format)
      goto cleanup_and_fail;
  } else {
    tqdm->params.bar_format = NULL;
  }

  if (user_params->colour) {
    tqdm->params.colour = strdup(user_params->colour);
    if (!tqdm->params.colour)
      goto cleanup_and_fail;
  } else {
    tqdm->params.colour = NULL;
  }

  if (user_params->postfix) {
    tqdm->params.postfix = strdup(user_params->postfix);
    if (!tqdm->params.postfix)
      goto cleanup_and_fail;
  } else {
    tqdm->params.postfix = NULL;
  }

  tqdm_load_env_vars(&tqdm->params);

  if (tqdm->params.mininterval < 0) {
    tqdm->params.mininterval = 0.1f; /* Default to 0.1 seconds */
  }
  if (tqdm->params.smoothing < 0 || tqdm->params.smoothing > 1) {
    tqdm->params.smoothing = 0.3f; /* Default smoothing */
  }
  if (tqdm->params.unit_divisor <= 0) {
    tqdm->params.unit_divisor = 1000.0f; /* Default divisor */
  }

  tqdm->current = begin;
  tqdm->end = end;
  tqdm->element_size = element_size;
  tqdm->count = 0;
  tqdm->n = tqdm->params.initial;

  if (tqdm->params.total == 0 && begin && end && element_size > 0) {
    size_t array_total = ((char *)end - (char *)begin) / element_size;
    tqdm->params.total = array_total;
  }
  tqdm->start_time = current_time_seconds();
  tqdm->last_print_time = tqdm->start_time;
  tqdm->last_print_count = tqdm->n;
  tqdm->closed = false;
  tqdm->paused = false;
  tqdm->pause_start = 0.0;
  tqdm->total_pause_time = 0.0;

  tqdm->rate_history_size = 10;
  tqdm->rate_history = calloc(tqdm->rate_history_size, sizeof(double));
  if (!tqdm->rate_history)
    goto cleanup_and_fail;
  tqdm->rate_history_idx = 0;

  tqdm->cached_rate = 0.0;
  tqdm->last_rate_calc_time = 0.0;
  tqdm->last_rate_calc_n = 0;
  tqdm->cached_terminal_width = 80;
  tqdm->last_terminal_check = 0.0;
  tqdm->display_buffer[0] = '\0';

  pthread_mutex_init(&tqdm->lock, NULL);

  tqdm->iterator_mode = false;
  tqdm->iterator_state = NULL;

  if (tqdm->params.delay > 0) {
    struct timespec delay_time;
    delay_time.tv_sec = (time_t)tqdm->params.delay;
    delay_time.tv_nsec =
        (long)((tqdm->params.delay - delay_time.tv_sec) * 1e9);
    nanosleep(&delay_time, NULL);
  }

  return tqdm;

cleanup_and_fail:
  free(tqdm->params.desc);
  free(tqdm->params.unit);
  free(tqdm->params.bar_format);
  free(tqdm->params.colour);
  free(tqdm->params.postfix);
  free(tqdm->rate_history);
  free(tqdm);
  return NULL;
}

tqdm_t *tqdm_create_iterator(void *iterator_state, void *(*next_func)(void *),
                             bool (*has_next_func)(void *),
                             void (*destroy_func)(void *)) {
  tqdm_t *tqdm = tqdm_create(NULL, NULL, 0);
  if (!tqdm)
    return NULL;

  tqdm->iterator_mode = true;
  tqdm->iterator_state = iterator_state;
  tqdm->next_func = next_func;
  tqdm->has_next_func = has_next_func;
  tqdm->destroy_func = destroy_func;

  return tqdm;
}

/* Iterator protocol */
tqdm_t *tqdm_iter(tqdm_t *tqdm) { return tqdm; }

bool tqdm_has_next(tqdm_t *tqdm) {
  if (tqdm->closed)
    return false;

  if (tqdm->iterator_mode) {
    return tqdm->has_next_func(tqdm->iterator_state);
  }

  if (tqdm->end) {
    return (char *)tqdm->current < (char *)tqdm->end;
  }

  if (tqdm->params.total > 0) {
    return tqdm->n < tqdm->params.total;
  }

  return true;
}

void *tqdm_next(tqdm_t *tqdm) {
  if (!tqdm_has_next(tqdm))
    return NULL;

  pthread_mutex_lock(&tqdm->lock);

  void *result = NULL;

  if (tqdm->iterator_mode) {
    result = tqdm->next_func(tqdm->iterator_state);
  } else {
    result = tqdm->current;
    tqdm->current = (char *)tqdm->current + tqdm->element_size;
  }

  tqdm->count++;
  tqdm->n++;

  tqdm_update_dynamic_miniters(tqdm);

  double current_time = current_time_seconds();
  bool should_print = false;

  bool is_complete =
      (tqdm->params.total > 0 && tqdm->n >= tqdm->params.total);

  if (is_complete ||
      (tqdm->params.miniters == 0 ||
       (tqdm->n - tqdm->last_print_count) >= tqdm->params.miniters)) {
    if (is_complete ||
        current_time - tqdm->last_print_time >= tqdm->params.mininterval) {
      should_print = true;
    }
  }

  if (should_print && !tqdm->params.disable) {
    tqdm_print_progress(tqdm);
  }

  pthread_mutex_unlock(&tqdm->lock);

  return result;
}

/* Update functions */
void tqdm_update(tqdm_t *tqdm) {
  if (!tqdm)
    return;
  tqdm_update_n(tqdm, 1);
}

void tqdm_update_n(tqdm_t *tqdm, size_t n) {
  if (!tqdm || tqdm->closed || tqdm->params.disable)
    return;

  pthread_mutex_lock(&tqdm->lock);

  tqdm->n += n;

  tqdm_update_dynamic_miniters(tqdm);

  double current_time = current_time_seconds();
  bool should_print = false;

  bool is_complete =
      (tqdm->params.total > 0 && tqdm->n >= tqdm->params.total);

  if (is_complete ||
      (tqdm->params.miniters == 0 ||
       (tqdm->n - tqdm->last_print_count) >= tqdm->params.miniters)) {
    if (is_complete ||
        current_time - tqdm->last_print_time >= tqdm->params.mininterval) {
      should_print = true;
    }
  }

  if (should_print) {
    tqdm_print_progress(tqdm);
  }

  pthread_mutex_unlock(&tqdm->lock);
}

bool tqdm_update_to(tqdm_t *tqdm, size_t n) {
  if (!tqdm || tqdm->closed || tqdm->params.disable)
    return false;

  pthread_mutex_lock(&tqdm->lock);

  size_t delta = (n > tqdm->n) ? (n - tqdm->n) : 0;
  tqdm->n = n;

  tqdm_update_dynamic_miniters(tqdm);

  double current_time = current_time_seconds();
  bool should_print = false;

  bool is_complete =
      (tqdm->params.total > 0 && tqdm->n >= tqdm->params.total);

  if (is_complete ||
      (tqdm->params.miniters == 0 || delta >= tqdm->params.miniters)) {
    if (is_complete ||
        current_time - tqdm->last_print_time >= tqdm->params.mininterval) {
      should_print = true;
    }
  }

  if (should_print) {
    tqdm_print_progress(tqdm);
  }

  pthread_mutex_unlock(&tqdm->lock);

  return should_print;
}

/* Core methods */
void tqdm_close(tqdm_t *tqdm) {
  if (!tqdm || tqdm->closed)
    return;

  pthread_mutex_lock(&tqdm->lock);

  tqdm_stop_monitor(tqdm);

  if (tqdm->params.leave && !tqdm->params.disable) {
    tqdm_print_progress(tqdm);
    fprintf(tqdm->params.file, "\n");
    fflush(tqdm->params.file);
  } else if (!tqdm->params.leave) {
    tqdm_clear(tqdm);
  }

  tqdm->closed = true;

  pthread_mutex_unlock(&tqdm->lock);
}

void tqdm_clear(tqdm_t *tqdm) {
  if (tqdm->params.disable)
    return;

  fprintf(tqdm->params.file, "\r\033[K");
  fflush(tqdm->params.file);
}

void tqdm_refresh(tqdm_t *tqdm) {
  if (tqdm->closed || tqdm->params.disable)
    return;

  pthread_mutex_lock(&tqdm->lock);
  tqdm_print_progress(tqdm);
  pthread_mutex_unlock(&tqdm->lock);
}

void tqdm_unpause(tqdm_t *tqdm) {
  if (!tqdm->paused)
    return;

  double current_time = current_time_seconds();
  tqdm->total_pause_time += current_time - tqdm->pause_start;
  tqdm->paused = false;
  tqdm->pause_start = 0.0;
}

void tqdm_reset(tqdm_t *tqdm, size_t total) {
  pthread_mutex_lock(&tqdm->lock);

  tqdm->n = tqdm->params.initial;
  tqdm->count = 0;
  tqdm->start_time = current_time_seconds();
  tqdm->last_print_time = tqdm->start_time;
  tqdm->last_print_count = tqdm->n;
  tqdm->total_pause_time = 0.0;
  tqdm->paused = false;

  if (total > 0) {
    tqdm->params.total = total;
  }

  memset(tqdm->rate_history, 0, tqdm->rate_history_size * sizeof(double));
  tqdm->rate_history_idx = 0;

  pthread_mutex_unlock(&tqdm->lock);
}

void tqdm_set_description(tqdm_t *tqdm, const char *desc) {
  tqdm_set_description_str(tqdm, desc, true);
}

void tqdm_set_description_str(tqdm_t *tqdm, const char *desc, bool refresh) {
  if (!tqdm)
    return;

  pthread_mutex_lock(&tqdm->lock);

  if (tqdm->params.desc) {
    free(tqdm->params.desc);
  }
  tqdm->params.desc = desc ? strdup(desc) : NULL;

  if (refresh) {
    tqdm_print_progress(tqdm);
  }

  pthread_mutex_unlock(&tqdm->lock);
}

void tqdm_set_postfix_str(tqdm_t *tqdm, const char *postfix, bool refresh) {
  if (!tqdm)
    return;

  if (tqdm->params.postfix) {
    free(tqdm->params.postfix);
  }
  tqdm->params.postfix = postfix ? strdup(postfix) : NULL;

  if (refresh) {
    tqdm_refresh(tqdm);
  }
}

void tqdm_write(const char *s, FILE *file, const char *end, bool nolock) {
  pthread_mutex_t *lock = tqdm_get_lock();

  if (!nolock) {
    pthread_mutex_lock(lock);
  }

  fprintf(file ? file : stdout, "\r\033[K%s%s", s, end ? end : "\n");
  fflush(file ? file : stdout);

  if (!nolock) {
    pthread_mutex_unlock(lock);
  }
}

void tqdm_display(tqdm_t *tqdm, const char *msg, int pos) {
  if (tqdm->params.disable)
    return;

  const char *display_msg = msg ? msg : "";

  if (pos >= 0 && tqdm->params.position >= 0) {
    fprintf(tqdm->params.file, "\033[%dA\r", pos);
  }

  fprintf(tqdm->params.file, "\r\033[K%s", display_msg);

  if (pos >= 0 && tqdm->params.position >= 0) {
    fprintf(tqdm->params.file, "\033[%dB", pos);
  }

  fflush(tqdm->params.file);
}

/* Context managers */
tqdm_external_write_context_t *tqdm_external_write_mode(FILE *file,
                                                        bool nolock) {
  tqdm_external_write_context_t *ctx =
      malloc(sizeof(tqdm_external_write_context_t));
  ctx->tqdm = NULL;
  ctx->original_file = file ? file : stdout;
  ctx->lock_acquired = false;

  if (!nolock) {
    pthread_mutex_lock(tqdm_get_lock());
    ctx->lock_acquired = true;
  }

  return ctx;
}

void tqdm_external_write_mode_exit(tqdm_external_write_context_t *ctx) {
  if (ctx->lock_acquired) {
    pthread_mutex_unlock(tqdm_get_lock());
  }
  free(ctx);
}

tqdm_wrapattr_context_t *tqdm_wrapattr(FILE *stream, const char *method,
                                       size_t total, bool bytes,
                                       tqdm_params_t *params) {
  tqdm_wrapattr_context_t *ctx = malloc(sizeof(tqdm_wrapattr_context_t));
  ctx->stream = stream;
  ctx->method = strdup(method);

  tqdm_params_t wrap_params = params ? *params : tqdm_default_params();
  wrap_params.total = total;
  if (bytes) {
    wrap_params.unit = strdup("B");
    wrap_params.unit_scale = true;
    wrap_params.unit_divisor = 1024;
  }

  ctx->tqdm = tqdm_create_with_params(NULL, NULL, 1, &wrap_params);

  ctx->read_func = NULL;
  ctx->write_func = NULL;

  return ctx;
}

void tqdm_wrapattr_exit(tqdm_wrapattr_context_t *ctx) {
  if (ctx->tqdm) {
    tqdm_close(ctx->tqdm);
    tqdm_destroy(ctx->tqdm);
  }
  free((void *)ctx->method);
  free(ctx);
}

/* Cleanup and destroy */
void tqdm_destroy(tqdm_t *tqdm) {
  if (!tqdm)
    return;

  if (!tqdm->closed) {
    tqdm_close(tqdm);
  }

  if (tqdm->params.desc)
    free(tqdm->params.desc);
  if (tqdm->params.unit)
    free(tqdm->params.unit);
  if (tqdm->params.bar_format)
    free(tqdm->params.bar_format);
  if (tqdm->params.colour)
    free(tqdm->params.colour);
  if (tqdm->params.postfix)
    free(tqdm->params.postfix);

  if (tqdm->rate_history) {
    free(tqdm->rate_history);
  }

  if (tqdm->iterator_mode && tqdm->destroy_func) {
    tqdm->destroy_func(tqdm->iterator_state);
  }

  pthread_mutex_destroy(&tqdm->lock);

  free(tqdm);
}

static void tqdm_print_progress(tqdm_t *tqdm) {
  if (!tqdm || tqdm->params.disable || tqdm->closed)
    return;

  double current_time = current_time_seconds();
  double elapsed = current_time - tqdm->start_time - tqdm->total_pause_time;

  double rate = (elapsed > 1e-6) ? (double)tqdm->n / elapsed : 0.0;

  int ncols = tqdm->params.ncols;
  if (ncols <= 0 || tqdm->params.dynamic_ncols) {
    if (current_time - tqdm->last_terminal_check < 1.0) {
      ncols = tqdm->cached_terminal_width;
    } else {
      ncols = get_terminal_width();
      tqdm->cached_terminal_width = ncols;
      tqdm->last_terminal_check = current_time;
    }
  }

  char *meter = tqdm_format_meter(
      tqdm->n, tqdm->params.total, elapsed, ncols, tqdm->params.desc,
      tqdm->params.ascii, tqdm->params.unit, tqdm->params.unit_scale, rate,
      tqdm->params.bar_format, tqdm->params.postfix,
      (int)tqdm->params.unit_divisor, tqdm->params.initial,
      tqdm->params.colour);

  if (meter) {
    fprintf(tqdm->params.file, "\r%s", meter);
    fflush(tqdm->params.file);
    free(meter);
  }

  tqdm->last_print_time = current_time;
  tqdm->last_print_count = tqdm->n;
}

/* Stub for pandas integration, TODO in the future? */
void tqdm_pandas_register(tqdm_params_t *params) { (void)params; }

/* Range functions */
range_iterator_t *range_create(int n) {
  return range_create_with_bounds(0, n);
}

range_iterator_t *range_create_with_bounds(int start, int end) {
  return range_create_with_step(start, end, 1);
}

range_iterator_t *range_create_with_step(int start, int end, int step) {
  range_iterator_t *range = malloc(sizeof(range_iterator_t));
  range->current = start;
  range->total = end;
  range->step = step;
  return range;
}

void range_destroy(range_iterator_t *range) { free(range); }

bool range_has_next(range_iterator_t *range) {
  if (range->step > 0) {
    return range->current < range->total;
  } else {
    return range->current > range->total;
  }
}

int range_next(range_iterator_t *range) {
  if (!range_has_next(range)) {
    return range->current;
  }

  int value = range->current;
  range->current += range->step;
  return value;
}

range_iterator_t *trange(int n) { return range_create(n); }

range_iterator_t *trange_with_bounds(int start, int end) {
  return range_create_with_bounds(start, end);
}

range_iterator_t *trange_with_step(int start, int end, int step) {
  return range_create_with_step(start, end, step);
}