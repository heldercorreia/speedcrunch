# Testing SpeedCrunch

See [BUILDING.md](BUILDING.md) for build requirements, Qt selection, and build
configurations. Building the tests also requires the Qt Test module.

## Building and running tests

By default, only the application is built. To also build and run the tests, enable
`BUILD_TESTING` when configuring. Run these commands from the repository root:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --parallel --output-on-failure

On Windows, CTest adds the configured Qt kit's DLL directory to each test's
`PATH`. This lets the tests find libraries such as `Qt6Core.dll` without copying
them beside every executable. Rerun the configure command after updating an
existing checkout to apply this setting.

When running a test executable directly, add the matching Qt kit's `bin`
directory to `PATH` in that shell first. With `QT_ROOT` set as described in
[BUILDING.md](BUILDING.md), use this in Command Prompt:

    set "PATH=%QT_ROOT%\bin;%PATH%"

Or, for the example installation in Git Bash:

    export PATH="/c/Qt/6.12.0/msvc2022_64/bin:$PATH"

Adjust the path to match your installation. A Windows dialog reporting a missing
Qt DLL means the test could not start, rather than that a test assertion failed.

The UI test suites use temporary configuration, session and cache directories,
and reset settings before each test case. They suppress the first-run number
format prompt. An unexpected prompt fails the test instead of blocking the suite.

CMake saves this option in the build directory's cache. Configure again with
`-DBUILD_TESTING=OFF` to return to application-only builds.

The shared window test library enables startup recording so appearance tests can
inspect the recorded work. This does not enable recording in the application.
Set `ENABLE_STARTUP_DIAGNOSTICS` explicitly for that, as described in
[DIAGNOSTICS.md](DIAGNOSTICS.md).
