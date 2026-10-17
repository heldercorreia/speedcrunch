# Building SpeedCrunch

## Requirements

To build SpeedCrunch, you need:

- A compiler toolchain with C17 and C++17 support
- [Qt](https://www.qt.io/) 6.x (Core, Widgets, Help, Network)
- [CMake](https://cmake.org/) 3.16 or later

## Getting the source

The source code is maintained on [GitHub](https://github.com/heldercorreia/speedcrunch).
To clone the repository:

    git clone https://github.com/heldercorreia/speedcrunch.git
    cd speedcrunch

## Build and install

To build SpeedCrunch in a dedicated build directory and install it, run the following
commands from the root of the source directory:

    cmake -S . -B build
    cmake --build build --config Release --parallel
    cmake --install build --config Release

## Build configurations

With Makefiles or ordinary Ninja, the project defaults to Release. Select another
configuration when configuring, for example with `-DCMAKE_BUILD_TYPE=Debug`.
The `--config Release` option above selects the configuration for generators
that support several configurations in one build directory, such as Xcode,
Visual Studio, and Ninja Multi-Config. It has no effect with Makefiles or
ordinary Ninja.

With Makefiles or ordinary Ninja, application and test executables are placed
directly under the chosen build directory. The application is `build/speedcrunch`
on Linux or `build/SpeedCrunch.app` on macOS. Multi-configuration generators add
a configuration subdirectory, such as `build/Release`.

To keep separate Release and Debug builds with Makefiles or ordinary Ninja,
choose a build directory for each configuration:

    cmake -S . -B build/Release -DCMAKE_BUILD_TYPE=Release
    cmake --build build/Release --parallel

    cmake -S . -B build/Debug -DCMAKE_BUILD_TYPE=Debug
    cmake --build build/Debug --parallel

These build output locations do not change the installation directories.

Use a fresh build directory if an existing one was configured before the root
CMake entry point was added. CMake saves the source directory in its cache.

## Building and running tests

See [TESTING.md](TESTING.md) for test requirements and instructions on building
and running the tests.

## Selecting Qt

When building against a Qt version that is not the system default Qt installation,
point CMake towards the Qt installation to use by setting `CMAKE_PREFIX_PATH` or
`Qt6_DIR` when running CMake.

### macOS

SpeedCrunch targets macOS 14.4 (Sonoma) and later. Official macOS releases build
against the official Qt libraries from the Qt Online Installer and ship those
same libraries and the required plugins in the application bundle. This keeps
the libraries' minimum macOS requirements aligned with SpeedCrunch's supported
versions.

Follow the [official Qt for macOS guide](https://doc.qt.io/qt-6/macos.html)
to install Xcode and Qt, then select the macOS desktop kit in the Qt Online
Installer. The SDK used to compile SpeedCrunch can be newer than the deployment
target. For example, a build using the macOS 27 SDK can still target macOS 14.4.

Homebrew's Qt libraries and their dependencies may require a newer macOS version.
During compatibility checks, Homebrew's QtHelp 6.12.0 library declared macOS 26.0
as its minimum, while the official QtHelp library declared 14.4. Lowering
SpeedCrunch's deployment target does not lower the requirements of its bundled
libraries. Use the official Qt installation for release builds to avoid these
dependency mismatches.

For example, if the official Qt kit is installed at `$HOME/Qt/6.12.0/macos`, run:

    "$HOME/Qt/6.12.0/macos/bin/qt-cmake" -S . -B build
    cmake --build build --config Release --parallel

The `qt-cmake` wrapper selects its own Qt installation automatically. Use a fresh
build directory when switching from Homebrew's Qt, since CMake caches the paths
to individual Qt modules. Adjust the kit path above to match your installation.

When packaging, use `macdeployqt` from the same official Qt installation. Check
the finished bundle's library requirements and test it on the oldest supported
macOS version before release.

## Build options

You can customize the build using the following variables. These are specified when
running CMake, in the form `cmake -S . -B build -Dvariable=value`.

- **CMAKE_BUILD_TYPE**: Select the configuration for Makefiles or ordinary Ninja,
  such as `Release`, `Debug`, or `RelWithDebInfo`. Defaults to `Release` in this
  project. Multi-configuration generators use `--config` when building instead.
- **CMAKE_OSX_DEPLOYMENT_TARGET**: Set the minimum macOS version for the build.
  Defaults to `14.4` unless an explicit target or the `MACOSX_DEPLOYMENT_TARGET`
  environment variable is supplied. Qt and all bundled dependencies must support
  the selected version or earlier.
- **BUILD_TESTING**: Set this to `ON` to build the test executables and register
  them with CTest. Defaults to `OFF`. See [TESTING.md](TESTING.md).
- **ENABLE_UPDATE_CHECKS**: Set this to `ON` to enable update checks on launch
  and the `Help > Check for Updates` menu entry. Defaults to `OFF`.
- **ENABLE_STARTUP_DIAGNOSTICS**: Set this to `ON` to record startup timings and
  detailed appearance diagnostics. Defaults to `OFF`. System information and
  settings remain available in `Help > App Diagnostics` in either build.
  See [DIAGNOSTICS.md](DIAGNOSTICS.md) for diagnostic beta build instructions.
- **PORTABLE_SPEEDCRUNCH**: Set this to `on` to have the application settings stored
  in the same location as the executable, e.g. for running from a USB drive without
  requiring installation.
- **CMAKE_INSTALL_PREFIX**: Change the installation prefix for SpeedCrunch.
- **CMAKE_INSTALL_BINDIR**: Change the executable directory on Unix systems except
  macOS and Haiku. Defaults to `bin` under the installation prefix.
- **CMAKE_INSTALL_DATAROOTDIR**: Change the shared data root on Unix systems except
  macOS and Haiku. Defaults to `share` under the installation prefix.
- **HTML_DOCS_DIR**: Change the path to the HTML manual that's embedded in the binary
  by the build. By default, a bundled prebuilt copy is used to minimize dependencies.

Official SpeedCrunch builds enable update checks explicitly:

    cmake -S . -B build -DENABLE_UPDATE_CHECKS=ON

These builds check for updates on launch at most once every 24 hours. Checks
report upstream releases and link to the SpeedCrunch website. They do not
install updates.

Package providers should leave this option off so users update through their
provider. CMake saves the option in the build directory's cache. Configure again
with `-DENABLE_UPDATE_CHECKS=OFF` to disable it in an existing build directory.

For qmake builds, add `CONFIG+=enable_update_checks` to enable the feature.

## Unix installation and packaging

On Linux and other Unix systems except macOS and Haiku, installation uses CMake's
`GNUInstallDirs` conventions. `CMAKE_INSTALL_BINDIR` and
`CMAKE_INSTALL_DATAROOTDIR` are relative to `CMAKE_INSTALL_PREFIX` unless explicitly
configured as absolute paths. For example:

    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_INSTALL_DATAROOTDIR=share
    cmake --build build --config Release --parallel
    DESTDIR=/tmp/speedcrunch-package cmake --install build --config Release

`DESTDIR` stages the installation under another directory for packaging. With
the example above, the executable is staged under
`/tmp/speedcrunch-package/usr/bin`. Desktop entries and application metadata are
installed under `applications` and `metainfo` within the shared data root.

Desktop icons are installed in the `hicolor` theme: the SVG in
`icons/hicolor/scalable/apps` and PNGs in
`icons/hicolor/<size>x<size>/apps` for sizes 16, 22, 24, 32, 48, 64, 128, and 256.
The installed icon basename is `org.speedcrunch.SpeedCrunch`. The PNG files are
included in the source distribution; building and installing SpeedCrunch does
not require icon-generation tools.

## Building the manual

Building the HTML manual is normally not necessary because a prebuilt copy is included
with the SpeedCrunch source. For more information, see the [manual's README](doc/src/README.md).
