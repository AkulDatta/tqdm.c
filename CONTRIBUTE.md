# Contributing

We welcome bug reports, feature requests, and pull requests.

1.  **Issues**: Please [open an issue](https://github.com/tqdm/tqdm.cpp/issues) to report a problem or suggest a feature.
2.  **Pull Requests**:
    *   Fork the repository.
    *   Create a new branch for your changes.
    *   Ensure your code is formatted with clang-format (`make cfmt`) correctly.
    *   Open a pull request with a clear description of your changes.

## Build Process

The project uses a standard `CMake` workflow. From the project root:

```sh
mkdir build && cd build
cmake ..
make
ctest
```
This will build the library, CLI, and test binaries, then run the test suite.