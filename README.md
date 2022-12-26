![Nautograf](title.svg)

## Introduction

Nautograf is a viewer for marine vector charts. It is inspired by [OpenCPN](https://www.opencpn.org) which is the only nautical chart software that is open source and runs with offline chart data. The motivation with Nautograf is to provide an application similar to OpenCPN, but provide a smoother zooming and panning experience, better rendering quality and in general a more modern code base.

Because getting chart data is not free for many geographic areas, Nautograf only supports oesu charts from [o-charts.org](https://www.o-charts.org). This format primarily adds some bells and whistles on top of the S-57 data model to support the OpenCPN and o-charts.org infrastructure.

## Development status

![Windows](https://github.com/hornang/nautograf/actions/workflows/windows.yaml/badge.svg)

Nautograf is a spare time project so progress is varying and non-deterministic. It is likely that it will never come up to a point of being sufficient for navigation. The current feature set is limited and **not sufficient for nautical navigation**. 

Nonetheless, the current implementation can display the most prominent S-57 features such as land areas, depth areas and depth numbers. Chart navigation works by using conventional _zoom to cursor_ or pinch zoom on touch capable devices.

If you are running Windows you can fetch the latest release in the [Microsoft Store](https://apps.microsoft.com/store/detail/nautograf/9NP97HF6LW08). But note that the release is usually much older than the code in the `main` branch.

Nautograf only supports _oesu_ charts from [o-charts.org](https://www.o-charts.org). Nautograf uses OpenCPN's decryption engine _oexserverd_ to decrypt the charts on the fly. The decryption mechanism is only implemented in the Windows version of Nautograf. You can read more about that in the [oesenc-export](https://github.com/hornang/oesenc-export) repository.

## Windows build instructions

To build Nautograf you will need CMake, Visual Studio 2019, Qt and vcpkg. If you are familiar with QtCreator that is the easiest way to get it built and debug any problems. You can also have a look on how it [builds with Github Actions](.github/workflows/windows.yaml).

A basic recipe for manual build in a cmd window follows here: (adjust paths as neccessary):

* Make Visual Studio compiler available. This should also make `cmake` available.
  * `"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"`.
* Run cmake configure with with the preset `ninja-multi-vcpkg` from `CMakePresets.json`.
  * `cmake --preset ninja-multi-vcpkg`
* Build it: `cmake --build --preset win-release`.
* Enter the build folder: `cd builds\ninja-multi-vcpkg\RelWithDebInfo`
* Use the `windeployqt` to bring in the required Qt DLLs and resources into the build folder: 
  * `C:\Qt\6.2.0\msvc2019_64\bin\windeployqt.exe -qmldir ..\..\..\qml nautograf.exe`
* Run it: `nautograf.exe`

## Design

Most online map viewers are based on [tiled web maps](https://en.wikipedia.org/wiki/Tiled_web_map). Tiling is an important feature to support smooth zooming and panning for the user. Online map viewers usually renders tiles that are already generated and hosted from a server. Nautograf is also tile based, but has to create those tiles internally because the chart data is provided by the user.

Because tile creation can take some time it runs in multiple threads. Each tile being generated is put in a queue and waits for the next available thread. Generated tile data is cached to disk. This means that each time you navigate to a *new area* it will take some time before the area shows. Keeping this delay as low as possible is a key factor to improve usability of the application.

### Internal tile format

Before tiles are rendered the source data in S-57 format is transformed to a [Cap'n Proto](https://capnproto.org/) based format. The internal format is defined in a [schema file](src/tilefactory/chartdata.capnp). As with most serialization frameworks for statically typed languages it is based on code generation. The schema defines C++ classes and most importantly the serialization/deserialization code. This provides both a way of caching data to disk and also an internal data model used when rendering. On windows you will find the cached tiles under `C:\Users\Username\AppData\Local\nautograf\cache\tiles`. There is no garbage collection so delete the data if it gets too large.

### Rendering

At the start of the project the tiles were rendered on the CPU using [QPainter](https://doc.qt.io/qt-6/qpainter.html). Using QPainter provides high quality results, but is too slow for a proper pan/zoom experience. Therefore the render code is currently being re-implemented using [Qt's scene graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html).

## Background

Electronic nautical charts have historically not been given much attention to by open source or web technology. Nautical navigation is indeed a niche for the average consumer that mostly navigates cities and roads. As a result nautical chart technology is lagging years behind well-known services like Google Maps and OpenStreetMaps.

Access to free nautical chart data is also still quite limited and depends on authorities' willingness to provide map data to the public. The lack of free public nautical data creates a burden for non-profit research and innovation at sea.

The only fully featured open source navigation tool for nautical maps today is [OpenCPN](https://www.opencpn.org). To resolve the lack of map data OpenCPN vendors in 2013 created the o-charts.org store where you can buy charts in OpenCPN's custom charts format. o-charts receive data from hydrographic offices and adds an end user encryption that gets decrypted in the closed source part of the OpenCPN's plugin [o-charts_pi](https://github.com/bdbcat/o-charts_pi) (previously called oesenc_pi).
