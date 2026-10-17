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

Alternatively, Qt 6 provides a `qt-cmake` wrapper in the selected kit's `bin`
directory (`qt-cmake.bat` on Windows, `qt-cmake` on macOS and Linux). It forwards
the Qt installation location to CMake, so you do not need to set
`CMAKE_PREFIX_PATH` yourself. See Qt's
[CMake setup guide](https://doc.qt.io/qt-6/cmake-making-qt-available.html).

### Windows

The steps below build a 64-bit application with Visual Studio 2022 and the
matching Qt desktop kit. See the [Qt for Windows guide](https://doc.qt.io/qt-6/windows.html)
for the Windows versions and compilers supported by your chosen Qt release.

1. Install Visual Studio 2022 or Build Tools for Visual Studio 2022. Select the
   **Desktop development with C++** workload, including the MSVC v143 x64/x86
   build tools and a Windows SDK. See Microsoft's
   [C++ command-line tools guide](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170).
2. Install Qt through the Qt Online Installer. Select the **MSVC 2022 64-bit**
   desktop kit for your Qt 6 version, with the Core, Widgets, Help, and Network
   modules. A MinGW kit cannot be used with the MSVC commands below.
3. Select **CMake** under **Developer and Designer Tools** in the Qt Online
   Installer. You can also install it separately from [cmake.org](https://cmake.org/download/).
   The Visual Studio 2022 generator requires CMake 3.21 or later, even though the
   project itself accepts older CMake versions. Install Git as well if you want
   to clone the repository using the commands above.

Open **x64 Native Tools Command Prompt for VS 2022** from the Start menu. This
sets up the compiler and Windows SDK tools, including `rc.exe`. Change to the
repository root, then run these commands in that prompt (`cmd.exe` syntax):

    set "QT_ROOT=C:\Qt\6.12.0\msvc2022_64"
    set "PATH=C:\Qt\Tools\CMake_64\bin;%QT_ROOT%\bin;%PATH%"
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_ROOT%"
    cmake --build build --config Release --parallel

For Git Bash or another Bash-compatible shell, use `export` and `$QT_ROOT`
instead of `set` and `%QT_ROOT%`. Start the shell from the Visual Studio
developer prompt if it needs to inherit the compiler and Windows SDK environment.
For example, the equivalent Git Bash commands are:

    export QT_ROOT='C:/Qt/6.12.0/msvc2022_64'
    export PATH="/c/Qt/Tools/CMake_64/bin:/c/Qt/6.12.0/msvc2022_64/bin:$PATH"
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$QT_ROOT"
    cmake --build build --config Release --parallel

Git Bash uses `/c/` for the C: drive in `PATH`, with colons between entries.
The remaining Windows examples use `cmd.exe` syntax. In Git Bash, replace
`%QT_ROOT%` with `$QT_ROOT`, `%CD%` with `$PWD`, and path separators with forward
slashes. For example, run the application with `build/Release/speedcrunch.exe`
and the deployment tool with `"$QT_ROOT/bin/windeployqt.exe"`.

Adjust `QT_ROOT` and the CMake directory on `PATH` to match your installation.
If CMake is already on `PATH`, you only need to add Qt's `bin` directory. The
compiler, Qt kit, and target architecture must match. Use a fresh build directory
when switching compilers, architectures, or Qt installations so CMake does not
reuse cached paths.

The executable is `build\Release\speedcrunch.exe`. You can run it from the same
prompt, where Qt's DLLs are on `PATH`:

    build\Release\speedcrunch.exe

To prepare a separate application folder, install into a local directory and
run `windeployqt` from the same Qt kit to collect any additional Qt dependencies:

    cmake --install build --config Release --prefix "%CD%\build\install"
    "%QT_ROOT%\bin\windeployqt.exe" --release --no-compiler-runtime build\install\speedcrunch.exe
    build\install\speedcrunch.exe

The install step copies the application, Qt libraries and plugins, and the MSVC
runtime files selected by CMake through `InstallRequiredSystemLibraries`.
`--no-compiler-runtime` tells `windeployqt` to leave compiler runtime deployment
to CMake, while still collecting Qt dependencies. It does not remove the
application's dependency on the MSVC runtime. When distributing the folder,
also ensure the target machine has the matching
[Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).

To generate an installer from the Release build above, install
[NSIS](https://nsis.sourceforge.io/) and make `makensis.exe` available on `PATH`.
The default Windows package generator is NSIS. From the repository root, change
to the build directory and run the Visual Studio `PACKAGE` target:

    cd build
    cmake --build . --config Release --target PACKAGE
    cd ..

The installer is written to `build`.

To generate a portable ZIP archive instead, configure a separate build directory
with `-DPORTABLE_SPEEDCRUNCH=ON -DCPACK_GENERATOR=ZIP`. From the repository root,
build the application, then run the same `PACKAGE` target in that build directory:

    cmake -S . -B build-portable -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QT_ROOT%" -DPORTABLE_SPEEDCRUNCH=ON -DCPACK_GENERATOR=ZIP
    cmake --build build-portable --config Release --parallel
    cd build-portable
    cmake --build . --config Release --target PACKAGE
    cd ..

The ZIP archive is written to `build-portable`. CPack assembles it using the
project's install rules, so a separate `cmake --install` command is not needed.
Portable mode stores settings and user data beside the executable. Selecting
the ZIP generator explicitly avoids requiring NSIS to create this package.

Check the application on a Windows machine without Qt installed before release,
including the embedded manual and update checks if enabled. See Qt's
[Windows deployment guide](https://doc.qt.io/qt-6/windows-deployment.html)
for details.

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

When packaging, use `macdeployqt` from the same official Qt installation.
The DMG script and CMake install step bundle only the SQLite SQL driver needed
by Qt Help, while preserving the other plugin groups used by the application.
This avoids pulling in unrelated database client libraries.

To generate an Apple Silicon DMG, use [pkg/generate-macos-dmg.sh](pkg/generate-macos-dmg.sh).
The script configures and builds an arm64 Release application in `build-arm64`,
bundles Qt with `macdeployqt`, applies and verifies an ad hoc signature, then
creates the DMG with CPack. From the repository root, put the official Qt kit's
`bin` directory on `PATH` so `qt-cmake` and `macdeployqt` use the same kit:

    export PATH="$HOME/Qt/6.12.0/macos/bin:$PATH"
    bash pkg/generate-macos-dmg.sh

The script configures with `qt-cmake`, which selects its Qt installation
automatically. It still uses `cmake` to build and `cpack` to package, so both
must also be on `PATH`. Adjust the Qt path to match your installation, and start
with a fresh `build-arm64` directory when switching Qt installations.
The output is `build/dmg/SpeedCrunch-<version>-arm64.dmg`.
The script does not notarize the application.

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
