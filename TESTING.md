# Testing SpeedCrunch

See [BUILDING.md](BUILDING.md) for build requirements, Qt selection, and build
configurations. Building the tests also requires the Qt Test module.

## Building and running tests

By default, only the application is built. To also build and run the tests, enable
`BUILD_TESTING` when configuring, including when using a build variant. Run these
commands from the repository root:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release -LE "^ui$" --parallel 4 --output-on-failure
    ctest --test-dir build -C Release -L "^ui$" --parallel 1 --output-on-failure

The UI test suites use temporary configuration, session and cache directories,
and reset settings before each test case. They suppress the first-run number
format prompt. An unexpected prompt fails the test instead of blocking the suite.
They use Qt's offscreen platform unless `QT_QPA_PLATFORM` is already set.

Run non-UI tests in parallel and UI tests one at a time so focus and the clipboard
cannot interfere between suites. All CI build jobs use this split and explicitly
set `BUILD_TESTING=ON`, including the Windows and macOS packaging jobs.

CMake saves `BUILD_TESTING` in the build directory's cache. It defaults to `OFF`
for every variant. Configure again with `-DBUILD_TESTING=OFF` to return to
application-only builds. No variant overrides explicit or cached values.
See [BUILDING.md](BUILDING.md#build-variants).

The shared window test library enables startup recording so appearance tests can
inspect the recorded work. This does not enable recording in the application.
Set `ENABLE_STARTUP_DIAGNOSTICS` explicitly or use `BUILD_VARIANT_DEV_DEBUG`
for that, as described in [DIAGNOSTICS.md](DIAGNOSTICS.md).

## Startup load tests

Builds with both `BUILD_TESTING=ON` and `ENABLE_STARTUP_DIAGNOSTICS=ON` include
four startup scenarios in every configuration, including Debug and Release.
Each workload runs once with the Classic style preset and once with Modern:

- `startup-load-baseline-classic`
- `startup-load-baseline-modern`
- `startup-load-heavy-classic`
- `startup-load-heavy-modern`

From the repository root, configure and run a Release build:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DENABLE_STARTUP_DIAGNOSTICS=ON
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release -L "^startup-load$" --parallel 1 --verbose

Include your Qt selection options from [BUILDING.md](BUILDING.md) when configuring.
Use the directory containing your CMake build in place of `build`. For a
developer Debug build, use `BUILD_VARIANT_DEV_DEBUG=ON` when configuring and
`Debug` in place of `Release` in the build and test commands. That variant
enables startup diagnostics automatically. Keep `-DBUILD_TESTING=ON` to build
the tests.

The baseline restores one tab with 10 history entries and hides the docks and
keypad. The heavy workload restores 30 tabs with 1,000 entries each, spread
across six panes in a two-column, three-row grid, with all eight docks and the
scientific keypad enabled at 50% zoom. All four docking areas contain separate
panels. The saved window targets 1,280 by 800 pixels and fits the available screen.

Each preset sets the editor, keypad, menu, dock and status bar appearance before
launch. The tests verify that the application recognizes the selected preset.
The session data and saved layout are the same for both presets.

The fixtures are generated on the fly by `prepareWorkload()` and `savedWindow()`
in [src/tests/teststartupload.cpp](src/tests/teststartupload.cpp), rather than
stored as predefined configuration files. Generation is deterministic: each
workload and preset produces the same session data and configured settings.
Window geometry adapts to the available screen and the controls' minimum sizes.
Temporary directory paths vary, and measured timings naturally vary between runs.

Each scenario writes its configuration and session files to a temporary
directory, then starts a fresh process through the application's normal startup
code. In the test build, `SPEEDCRUNCH_UI_TEST_STORAGE` redirects configuration,
session and cache paths to that directory. Real user settings are untouched,
and the temporary directory is removed after the run. Update checks are disabled
for these runs. The scenarios use the native platform plugin unless
`QT_QPA_PLATFORM` is already set.

The tests check the restored sessions, history counts and control visibility.
They also check pane visibility, dock placement and whether the window fits on
screen. Each case writes a text diagnostic report, a JSON timing summary and a
window screenshot under `startup-diagnostics` in the build directory, named for
the workload and preset, such as `heavy-classic.txt`, `heavy-classic.json`
and `heavy-classic.png`. The JSON summary includes the preset name.

Compare Classic and Modern results for the same workload and build configuration.
Single runs can vary, so repeat the tests before drawing performance conclusions.
Timings include application initialization and session restoration.
Fixture generation and report writing
are outside the measurement. These are fresh-process launches, without clearing
the operating system's file cache. Timings have no pass/fail threshold.

Windows Debug CI runs all four scenarios with the native Windows platform plugin
and uploads the reports and test logs even when a scenario fails. Ordinary
application launches do not select either workload.

## Windows

Follow the [Windows build setup](BUILDING.md#windows), including setting
`QT_ROOT` to your Qt MSVC kit. From the repository root in **x64 Native Tools
Command Prompt for VS 2022**, configure a developer Debug build and run the tests:

    cmake -S . -B build-dev -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DBUILD_VARIANT_DEV_DEBUG=ON -DBUILD_TESTING=ON
    cmake --build build-dev --config Debug --parallel
    ctest --test-dir build-dev -C Debug -LE "^ui$" --parallel 4 --output-on-failure
    ctest --test-dir build-dev -C Debug -L "^ui$" --parallel 1 --output-on-failure

Rebuild after pulling test or application changes. To rerun only the suites that
failed in the previous CTest run, use:

    ctest --test-dir build-dev -C Debug --rerun-failed --parallel 1 --output-on-failure

The build and test commands also work in Git Bash once the build is configured.
The configure command above uses Command Prompt syntax for `QT_ROOT`.

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

### Offscreen fonts

No `QT_QPA_PLATFORM` or `QT_QPA_FONTDIR` setting is needed for a normal test run.
When using offscreen mode on Windows, the UI test fixture sets `QT_QPA_FONTDIR`
to `%SystemRoot%\Fonts` if that directory exists and no font directory is already
set. An explicit `QT_QPA_FONTDIR` value takes precedence.

If Qt reports `Cannot find font directory`, check that the directory exists and
contains font files. To select a different directory, set it before running
CTest. For example, in Command Prompt:

    set "QT_QPA_FONTDIR=C:\Windows\Fonts"
    ctest --test-dir build-dev -C Debug -L "^ui$" --parallel 1 --output-on-failure

Or in Git Bash:

    QT_QPA_FONTDIR=C:/Windows/Fonts ctest --test-dir build-dev -C Debug -L "^ui$" --parallel 1 --output-on-failure

Missing fonts can cause failures in text sizing, glyph rendering and font dialog
tests. Offscreen warnings such as `This plugin does not support raise()` or
`propagateSizeHints()` are expected and do not by themselves mean a test fails.

### Native Windows UI checks

To check the native Windows backend, use a separate shell and run UI suites one
at a time so desktop focus and the clipboard cannot interfere. In Command Prompt:

    set "QT_QPA_PLATFORM=windows"
    ctest --test-dir build-dev -C Debug -L ui --parallel 1 --output-on-failure

Or in Git Bash:

    QT_QPA_PLATFORM=windows ctest --test-dir build-dev -C Debug -L ui --parallel 1 --output-on-failure

Close the Command Prompt shell when finished, or clear `QT_QPA_PLATFORM` with
`set "QT_QPA_PLATFORM="` before returning to offscreen tests.
