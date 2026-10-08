# SpeedCrunch
SpeedCrunch is a high-precision scientific calculator.
It features a syntax-highlighted scrollable display and is designed to be fully used via keyboard. Some distinctive
features are auto-completion of functions and variables, a formula book, and quick
insertion of constants from various fields of knowledge. It is available for Windows, macOS,
and Linux in a number of languages.

Visit the [SpeedCrunch website](https://www.speedcrunch.org/) for
[downloads](https://www.speedcrunch.org/download.html) and the
[online manual](https://www.speedcrunch.org/introduction.html).

![SpeedCrunch screenshot](doc/src/screen1.png)

## Building
To build SpeedCrunch, you need:

- A compiler toolchain with C17 and C++17 support
- [Qt](https://www.qt.io/) 6.x (Core, Widgets, Help, Network; Test is required only
  when building the tests)
- [CMake](https://cmake.org/) 3.16 or later

To build SpeedCrunch in a dedicated build directory and install it, run the following
commands from the root of the source directory:

    cmake -S . -B build
    cmake --build build --config Release --parallel
    cmake --install build --config Release

Application and test executables are placed directly under `build`. On macOS,
the default build produces `build/SpeedCrunch.app`. Some generators use
configuration subdirectories, such as `build/Release`.

Use a fresh build directory if an existing one was configured before the root
CMake entry point was added. CMake saves the source directory in its cache.

By default, only the application is built. To also build and run the tests, enable
`BUILD_TESTING` when configuring:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release --parallel
    ctest --test-dir build -C Release --output-on-failure

CMake saves this option in the build directory's cache. Configure again with
`-DBUILD_TESTING=OFF` to return to application-only builds.

When building against a Qt version that is not the system default Qt installation,
point CMake towards the Qt installation to use by setting `CMAKE_PREFIX_PATH` or
`Qt6_DIR` when running CMake.

Example (Homebrew on macOS):

    brew install qt
    cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
    cmake --build build --config Release --parallel

You can customize the build using the following variables. These are specified when
running CMake, in the form `cmake -S . -B build -Dvariable=value`.

- **BUILD_TESTING**: Set this to `ON` to build the test executables and register
  them with CTest. Defaults to `OFF`.
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

### Unix installation and packaging

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

## File locations
SpeedCrunch uses [Qt's standard per-user locations](https://doc.qt.io/qt-6/qstandardpaths.html)
for persistent application data and configuration:

| Platform | Application data | Preferences/configuration |
| --- | --- | --- |
| macOS | `~/Library/Application Support/SpeedCrunch/` | `~/Library/Preferences/SpeedCrunch/` |
| Windows | `%APPDATA%\SpeedCrunch\` | `%APPDATA%\SpeedCrunch\` |
| Linux | `$XDG_DATA_HOME/SpeedCrunch/` (usually `~/.local/share/SpeedCrunch/`) | `$XDG_CONFIG_HOME/SpeedCrunch/` (usually `~/.config/SpeedCrunch/`) |

Qt respects system-specific overrides to these locations. In the Windows portable build,
application data and preferences/configuration are all stored in the same directory as
the portable application.

## Building the manual
Building the HTML manual is normally not necessary because a prebuilt copy is included
with the SpeedCrunch source. For more information, see the [manual's README](doc/src/README.md).

## Contributing
- Report bugs, request features or implement a ticket from the [issue tracker](https://speedcrunch.org/issues.html).
- Be part of the [community](https://speedcrunch.org/community.html).
- [Donate](https://www.speedcrunch.org/donate.html) to help supporting the expenses of server hosting, internet domain and computers for further development.

## License
This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with this program; see the file [LICENSE](LICENSE).  If not, write to the Free
Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
MA 02110-1301, USA.
