Note: These build instructions are outdated.
Look at the Little Navmap Github Wiki for constantly updated instructions:
https://github.com/albar965/littlenavmap/wiki/Compiling

==============================================================================

# Build Instructions

The atools static library is required. Clone atools (`git clone https://github.com/albar965/atools.git`)
and follow the build instructions there. The instructions in this `BUILD.txt` assume that atools was installed
and compiled successfully and Qt Creator was configured accordingly.

Clone the littlenavmap GIT repository into the same directory as atools. You can use another
directory but then you need to adapt the configuration by changing environment variables.

Look at the `littlenavmap.pro` file. There is a list of documented environment variables that
can be set to customize the build, like the inclusion of SimConnect or paths to the projects. Most
of these variables are optional and use a reasonable default value. Set these variables in the Qt
Creator GUI or on the command line.

There is no need to edit the `*.pro` files.

## Marble

The Marble widget is needed to compile littlenavmap.

Get Marble from my repository which contains a few required improvements:
`git clone https://github.com/albar965/marble.git`

Build and install it according to the included instructions here
https://github.com/albar965/littlenavmap/wiki/Compiling#compile-marble .
Use branch `lnm/1.1` for the Marble build.

A typical Marble build script for Windows minimizing uneeded features looks like below:

cd $APROJECTS/build-marble-release

cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_BUILD=TRUE -DQTONLY=TRUE -DBUILD_MARBLE_EXAMPLES=NO -DBUILD_INHIBIT_SCREENSAVER_PLUGIN=NO -DBUILD_MARBLE_APPS=NO -DBUILD_MARBLE_EXAMPLES=NO -DBUILD_MARBLE_TESTS=NO -DBUILD_MARBLE_TOOLS=NO -DBUILD_TESTING=NO -DBUILD_WITH_DBUS=NO -DMARBLE_EMPTY_MAPTHEME=YES -DMOBILE=NO -DWITH_DESIGNER_PLUGIN=NO -DWITH_Phonon=NO -DWITH_Qt5Location=NO -DWITH_Qt5Positioning=NO -DWITH_Qt5SerialPort=NO -DWITH_ZLIB=NO -DWITH_libgps=NO -DWITH_libshp=NO -DWITH_libwlocate=NO -DCMAKE_INSTALL_PREFIX=$APROJECTS/Marble-release -DEXEC_INSTALL_PREFIX=$APROJECTS/Marble-release -DCMAKE_PREFIX_PATH=$HOME/Qt/5.12.8/gcc_64/ ../marble/

Keep in mind that the Marble build is buggy on Windows and macOS and does not build a proper installation on `make install`.
You have to copy library and include files manually.

## Default paths and Environment Variables

The project can be built with almost no configuration changes since all project files fall back to sensible
default paths if the corresponding environment variables are not set. `APROJECTS` is the placeholder for the base directory in
the examples below. Avoid paths with spaces if possible.

* `APROJECTS/atools/src`                           Sources. `ATOOLS_INC_PATH`
* `APROJECTS/build-atools-debug`                   Debug build. `ATOOLS_LIB_PATH`
* `APROJECTS/build-atools-release`                 Release build. `ATOOLS_LIB_PATH`
* `APROJECTS/build-littlenavmap-debug`             Debug build.
* `APROJECTS/build-littlenavmap-release`           Release build.
* `APROJECTS/Marble-debug/include` and `.../lib`   Debug built by `make install`. `MARBLE_INC_PATH` and `MARBLE_LIB_PATH`
* `APROJECTS/Marble-release/include` and `.../lib` Release built by `make install`. `MARBLE_INC_PATH` and `MARBLE_LIB_PATH`
* `APROJECTS/deploy`                               Target for `make deploy`. `DEPLOY_BASE`
* `APROJECTS/little_navmap_db`                     Navigraph database copied from here for littlenavmap. Optional. `DATABASE_BASE`
* `APROJECTS/little_navmap_help`                   Additional PDF help files copied from here for littlenavmap. Optional. `HELP_BASE`
* `APROJECTS/littlenavmap`                         Little Navmap Sources.
* `C:\Program Files (x86)\...\SimConnect SDK`      SimConnect on Windows only. Optional. `ATOOLS_SIMCONNECT_PATH`
* `C:\Program Files (x86)\OpenSSL-Win32`           Default SSL installation path. Required on Windows only. `OPENSSL_PATH`

## Windows

- Get OpenSSL binaries from https://wiki.openssl.org/index.php/Binaries and extract them
  to the project directory.
  You might use any 1.1.1 version but have to adapt the OPENSSL_PATH if it differs.
- Install SimConnect if needed (optional). The FSX SP2 is the preferred version.
- Clone littlenavmap from GIT (`git clone https://github.com/albar965/littlenavmap.git`)
  to e.g.: `C:\Projects\littlenavmap`
- For littlenavmap use the build directory of e.g.: `C:\Projects\build-littlenavmap-release`. Otherwise
  change the paths with envronment variables (see `littlenavmap.pro` file).
- Import littlenavmap into the Qt Creator workspace (atools should be already there).
- Configure the project and enable the shadow build for release or debug versions.
- Set the environment variables `ATOOLS_SIMCONNECT_PATH` (optional if SimConnect needed) and/or
  `OPENSSL_PATH` (required on Windows) in the Qt Creator GUI.
- Set the build kit for atools and littlenavmap to MinGW 32bit.
- Run qmake from Qt Creator for all projects
- Build all projects from Qt Creator
- Create and run the target `deploy`. This will create a directory `DEPLOY_BASE\Little Navmap` with the program.

## Linux / macOS

Install Qt development packages. Version at least 5.6.

You can build the program on Linux or macOS similar to the Windows instructions above either using
the Qt Creator GUI or the command line.

SimConnect is not available on these platforms.
OpenSSL will be detected automatically by the build scripts.

The following assumes that atools and Marble was already installed and built.

### To build the littlenavmap release version:

```
mkdir build-littlenavmap-release
cd build-littlenavmap-release
qmake ../littlenavmap/littlenavmap.pro CONFIG+=release
make
```

### To build the littlenavmap debug version:

```
mkdir build-littlenavmap-debug
cd build-littlenavmap-debug
qmake ../littlenavmap/littlenavmap.pro CONFIG+=debug
make
```

## Branches / Project Dependencies

Make sure to use the correct branches to avoid breaking dependencies.
The branch master is the unstable development branch but all software should compile there.

For releases check the release/MAJOR.MINOR branches to get the correct dependencies.
The atools branch is one MAJOR number ahead.

So Little Navconnect branch `release/1.2` should work well with atools `release/2.2` for example.
