# tqdm.c – Tiny, Fast Progress-Bars for C

Minimal C11 (C99 OK) *unofficial* port of Python's tqdm.  No dependencies beyond the C standard library + pthreads.

## Build & Test

```bash
mkdir build && cd build
cmake .. && make -j4        # builds lib + CLI + tests
# ctest -V                    # runs unit + macro tests
```

## Quick Start

Automatic range iteration (à la Python):
```c
#include "tqdm/tqdm.h"

TQDM_FOR(int, i, 0, 100) {
    /* work */
}
```
Manual bar:
```c
TQDM_MANUAL(pbar, total) {
    for(size_t i=0;i<total;i++) {
        /* work */
        TQDM_UPDATE(pbar);
    }
}
```
CLI usage (acts like `pv`):
```bash
cat file | tqdm --bytes --desc "copying"
```

## Features
* Unicode or ASCII bars, colour, custom format.
* Thread-safe, zero malloc in hot path.
* Environment-variable config (e.g. `TQDM_MININTERVAL`).
* Tested via CTest (core + macro suites).

## Licence
MIT
