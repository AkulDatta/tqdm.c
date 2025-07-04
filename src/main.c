#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include "tqdm/tqdm.h"

#define VERSION "4.67.1"
#define DEFAULT_BUF_SIZE 8192

/* =============================
 * Printing helpers
 * ============================= */
static void print_version(void) {
    puts("tqdm " VERSION);
}

static void print_help(void) {
    puts("Usage: tqdm [OPTIONS]\n"
         "Monitor progress of data through a pipe.\n\n"
         "Core Options:\n"
         "  --desc=DESC               Prefix for the progress bar\n"
         "  --total=N                 Total expected items/bytes\n"
         "  --leave / --no-leave      Leave progress bar after completion (default: leave)\n"
         "  --file=[stdout|stderr|PATH] Output file (default: stderr)\n"
         "  --ncols=N                 Width of progress bar\n"
         "  --mininterval=F           Minimum update interval (s)\n"
         "  --miniters=N              Minimum iterations between refreshes\n"
         "  --ascii                   Use ASCII bar characters\n"
         "  --disable                 Disable the progress bar\n"
         "  --unit=UNIT               Unit text (default: it)\n"
         "  --unit-scale              Auto-scale units\n"
         "  --dynamic-ncols           Dynamically resize bar width\n"
         "  --smoothing=F             Rate smoothing factor\n"
         "  --bar-format=FMT          Custom bar format string\n"
         "  --initial=N               Initial counter value\n"
         "  --position=N              Line position for multi-bars\n"
         "  --postfix=STR             Postfix string\n"
         "  --unit-divisor=N          Unit divisor (1000/1024)\n"
         "  --colour=COLOR            Progress bar colour\n"
         "  --delay=F                 Initial delay before showing (s)\n\n"
         "Advanced Options:\n"
         "  --bytes                   Bytes mode (unit=B, scaled)\n"
         "  --delim=CHAR              Delimiter for text mode (default: \n)\n"
         "  --buf-size=N              I/O buffer size (default: 8192)\n"
         "  --tee                     Copy input to stdout as well\n"
         "  --update                  Treat each input line as an increment\n"
         "  --update-to               Treat each input line as an absolute value\n"
         "  --null                    Allow NUL bytes in tee output\n\n"
         "Other Options:\n"
         "  --help                    Show this help message\n"
         "  --version                 Show version information");
}

/* =============================
 * Processing options
 * ============================= */
typedef struct {
    char    delim;        /* Delimiter for counting ("lines")        */
    size_t  buf_size;     /* Buffer for fread                         */
    bool    tee;          /* Mirror input to stdout                   */
    bool    update;       /* Incremental numeric updates              */
    bool    update_to;    /* Absolute numeric updates                 */
    bool    null_ok;      /* Allow NUL bytes in tee output            */
} processing_opts_t;

static processing_opts_t proc_default(void) {
    return (processing_opts_t){ .delim='\n', .buf_size=DEFAULT_BUF_SIZE };
}

/* =============================
 * CLI parsing
 * ============================= */
static tqdm_params_t parse_args(int argc, char **argv, processing_opts_t *popts) {
    *popts = proc_default();
    tqdm_params_t p = tqdm_default_params();
    tqdm_load_env_vars(&p);

    static struct option long_opts[] = {
        /* Options */
        {"desc",          required_argument, 0, 'd'},
        {"total",         required_argument, 0, 't'},
        {"leave",         no_argument,       0, 'l'},
        {"no-leave",      no_argument,       0, 'L'},
        {"file",          required_argument, 0, 'f'},
        {"ncols",         required_argument, 0, 'c'},
        {"mininterval",   required_argument, 0, 'i'},
        {"miniters",      required_argument, 0, 'm'},
        {"ascii",         no_argument,       0, 'a'},
        {"disable",       no_argument,       0, 'D'},
        {"unit",          required_argument, 0, 'u'},
        {"unit-scale",    no_argument,       0, 'U'},
        {"dynamic-ncols", no_argument,       0, 'N'},
        {"smoothing",     required_argument, 0, 's'},
        {"bar-format",    required_argument, 0, 'b'},
        {"initial",       required_argument, 0, 'n'},
        {"position",      required_argument, 0, 'p'},
        {"postfix",       required_argument, 0, 'P'},
        {"unit-divisor",  required_argument, 0, 'v'},
        {"colour",        required_argument, 0, 'C'},
        {"delay",         required_argument, 0, 'y'},
        {"bytes",         no_argument,       0, 'B'},
        {"delim",         required_argument, 0, 'e'},
        {"buf-size",      required_argument, 0, 'z'},
        {"tee",           no_argument,       0, 'T'},
        {"update",        no_argument,       0, 'R'},
        {"update-to",     no_argument,       0, 'S'},
        {"null",          no_argument,       0, 'x'},
        /* Info */
        {"help",          no_argument,       0, 'h'},
        {"version",       no_argument,       0, 'V'},
        {0,0,0,0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "d:t:lLf:c:i:m:aDu:UNs:b:n:p:P:v:C:y:Be:z:TRSxhV", long_opts, NULL)) != -1) {
        switch (opt) {
            /* Options */
            case 'd': free(p.desc);  p.desc=strdup(optarg);                break;
            case 't': p.total  = (size_t)atoll(optarg);                   break;
            case 'l': p.leave  = true;                                    break;
            case 'L': p.leave  = false;                                   break;
            case 'f': {
                if (!strcmp(optarg, "stdout"))      p.file = stdout;
                else if (!strcmp(optarg, "stderr")) p.file = stderr;
                else {
                    p.file = fopen(optarg, "w");
                    if (!p.file) fprintf(stderr, "Failed to open %s: %s\n", optarg, strerror(errno)), p.file = stderr;
                }
            } break;
            case 'c': p.ncols = atoi(optarg);                             break;
            case 'i': p.mininterval = atof(optarg);                       break;
            case 'm': p.miniters    = (unsigned)atoi(optarg);             break;
            case 'a': p.ascii  = true;                                    break;
            case 'D': p.disable= true;                                    break;
            case 'u': free(p.unit); p.unit = strdup(optarg);              break;
            case 'U': p.unit_scale = true;                                break;
            case 'N': p.dynamic_ncols = true;                             break;
            case 's': p.smoothing = atof(optarg);                         break;
            case 'b': free(p.bar_format); p.bar_format=strdup(optarg);    break;
            case 'n': p.initial = (size_t)atoll(optarg);                  break;
            case 'p': p.position = atoi(optarg);                          break;
            case 'P': free(p.postfix); p.postfix=strdup(optarg);          break;
            case 'v': p.unit_divisor = atof(optarg);                      break;
            case 'C': free(p.colour); p.colour=strdup(optarg);            break;
            case 'y': p.delay = atof(optarg);                             break;
            case 'B': /* bytes mode */
                p.unit_scale = true; p.unit_divisor = 1024.0f;
                free(p.unit); p.unit=strdup("B");
                break;
            case 'e':
                if (!strcmp(optarg,"\\n"))      popts->delim='\n';
                else if (!strcmp(optarg,"\\0")||!strcmp(optarg,"0")) popts->delim='\0';
                else                           popts->delim=optarg[0];
                break;
            case 'z': popts->buf_size = (size_t)atoll(optarg);            break;
            case 'T': popts->tee = true;                                  break;
            case 'R': popts->update = true;                               break;
            case 'S': popts->update_to = true;                            break;
            case 'x': popts->null_ok = true;                              break;
            /* Info */
            case 'h': print_help();  exit(0);
            case 'V': print_version(); exit(0);
            default : fprintf(stderr, "Unknown option. Try --help.\n"); exit(1);
        }
    }
    return p;
}

/* =============================
 * Processing helpers
 * ============================= */
static size_t process_updates(tqdm_t *bar, FILE *in, processing_opts_t *o) {
    char line[128];
    size_t processed = 0;
    while (fgets(line, sizeof(line), in)) {
        char *end; double val = strtod(line, &end);
        if (end==line) continue; /* not a number */
        if (o->update_to) tqdm_update_to(bar, (size_t)val);
        else              tqdm_update_n(bar,  (size_t)val);
        ++processed;
        if (o->tee && o->null_ok==false) puts(line);
    }
    return processed;
}

static size_t process_stream(tqdm_t *bar, FILE *in, processing_opts_t *o) {
    char *buf = malloc(o->buf_size);
    if (!buf) { perror("malloc"); return 0; }
    size_t processed = 0;
    size_t read;
    while ((read = fread(buf,1,o->buf_size,in))>0) {
        if (o->tee) fwrite(buf,1,read, stdout);
        /* Count delimiters (or bytes if delim==0) */
        if (o->delim=='\0') {
            tqdm_update_n(bar, read);
            processed += read;
                } else {
            for (size_t i=0;i<read;i++) if (buf[i]==o->delim) {
                tqdm_update(bar);
                ++processed;
            }
        }
    }
    free(buf);
    return processed;
}

/* =============================
 * Main function (Entry point)
 * ============================= */
int main(int argc, char **argv) {
    processing_opts_t proc_opts;
    tqdm_params_t params = parse_args(argc, argv, &proc_opts);

    /* Create progress bar */
    tqdm_t *bar = tqdm_create_with_params(NULL, NULL, 1, &params);
    if (!bar) { fputs("Failed to create tqdm instance\n", stderr); return 1; }

    FILE *input = stdin;
    if (isatty(STDIN_FILENO)) fputs("Reading from terminal (Ctrl+D to end)\n", stderr);

    size_t count = 0;
    if (proc_opts.update || proc_opts.update_to) count = process_updates(bar, input, &proc_opts);
    else                                         count = process_stream(bar,  input, &proc_opts);

    (void)count; /* count can be used for stats if desired */

    tqdm_close(bar);
    tqdm_destroy(bar);
    return ferror(input) ? 1 : 0;
} 