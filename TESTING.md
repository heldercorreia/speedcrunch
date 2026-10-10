# Testing SpeedCrunch

See [BUILDING.md](BUILDING.md) for build requirements, Qt selection, and build
configurations. Building the tests also requires the Qt Test module.

## Building and running tests

By default, only the application is built. To also build and run the tests, enable
`BUILD_TESTING` when configuring. Run these commands from the repository root:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --parallel --output-on-failure

The UI test suites use temporary configuration, session and cache directories,
and reset settings before each test case. They suppress the first-run number
format prompt. An unexpected prompt fails the test instead of blocking the suite.

CMake saves this option in the build directory's cache. Configure again with
`-DBUILD_TESTING=OFF` to return to application-only builds.
