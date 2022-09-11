
![Nautograf](title.svg)

![Windows](https://github.com/hornang/nautograf/actions/workflows/windows.yaml/badge.svg)

## Introduction

Nautograf is a chart viewer for marine chart data. It combines reading of such data with tiling techniques found in online map services. The ambition is to provide a similar user experience to the existing online map services without being dependent on web infrastructure.

The application is designed as a desktop application that can be installed alongside OpenCPN and read its [o-charts.org](https://www.o-charts.org) charts. The o-charts.org chart format is a wrapper around S-57 data which is the standard format used by hydrographic offices. Nautograf can only read charts from o-charts.org, but presumably not much effort should be needed to support pure S-57 format.

Nautograf can be considered derivative works of [OpenCPN](https://www.opencpn.org), but most of the code is written from scratch.

The C++ standard library, [Cap'n Proto](https://capnproto.org/) and [Clipper](http://www.angusj.com/delphi/clipper.php) are used for tile generation. [Qt 6](https://www.qt.io/) is used for rendering and user interface.

A Windows build is available in the [Microsoft Store](https://apps.microsoft.com/store/detail/nautograf/9NP97HF6LW08).

## Features

The feature set is very limited and absolutely **not sufficient for nautical navigation**. The focus so far has been on creating a framework for producing tiles from OpenCPN's/o-charts.org chart format.

The following S-57 objects are supported (at varying degree):

* [Coverage](http://www.s-57.com/Object.asp?nameAcr=M_COVR)
* [Land area](http://www.s-57.com/Object.asp?nameAcr=LNDARE)
* [Built-Up areas](http://www.s-57.com/Object.asp?nameAcr=BUAARE)
* [Depth areas](http://www.s-57.com/Object.asp?nameAcr=DEPARE)
* [Soundings](http://www.s-57.com/Object.asp?nameAcr=SOUNDG)
* [Roads](http://www.s-57.com/Object.asp?nameAcr=ROADWY)
* [Beacon, special purpose/general](http://www.s-57.com/Object.asp?nameAcr=BCNSPP)
* [Underwater rocks](http://www.s-57.com/Object.asp?nameAcr=UWTROC): ![](symbols/underwater_rocks/awash.svg) ![](symbols/underwater_rocks/always_submerged.svg) ![](symbols/underwater_rocks/cover_and_uncovers.svg)
* [Buoys, lateral](http://www.s-57.com/Object.asp?nameAcr=BOYLAT): ![](symbols/buoys/spar_starboard.svg) ![](symbols/buoys/spar_port.svg) ![](symbols/buoys/can.svg) etc.

Charts from o-charts.org are encrypted. Nautograf uses OpenCPN's decryption engine _oexserverd_ to decrypt charts on the fly, but it is only implemented on Windows. Read more about this in my other repository [oesenc-export](https://github.com/hornang/oesenc-export).

### Roadmap

Current rendering implementation uses QPainter to draw on QImages that are displayed on screen. There is little hardware acceleration possible with this approach and it is also very memory intensive. Some of the planned improvements are:

* Replace QPainter based rendering with Qt's scene graph.
  * Replace current text rendering with signed distance fields font atlas.
  * Replace current polygon rendering with triangles in scene graph.
* Pre-built binaries for Linux.
* Increase S-57 completeness.

## Windows build instructions

To build it you will need CMake, Visual Studio 2019, Qt and vcpkg. If you are familiar with QtCreator that is the easiest way to get it built and debug any problems. You can also have a look on how it [builds with Github Actions](.github/workflows/windows.yaml).

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

## How it works and why

Most tile based map viewers are based on [tiled web maps](https://en.wikipedia.org/wiki/Tiled_web_map). The goal for this project was to create a tile based nautical chart viewer that does not depend on web infrastructure. Once you get the application independent chart data you do not need Internet. A large part of the code base revolves around a [tilefactory](src/tilefactory) that generates and serves chart tiles internally. The tilefactory is written in plain C++ and is not dependent on Qt.

Since tile creation can take some time it runs in multiple threads provided via the `QtConurrent` module. Each tile being rendered is put in a queue and waits for the next available thread. Already generated tile data is cached to disk. This means that each time you navigate to a *new area* it will take some time before the area shows. Keeping this delay as low as possible is a key factor to improve usability of the application. Polygon clipping is by far the most time consuming operation.

### Chart data management

oesu chart data comes in many files that covers different areas with different resolutions. Some files may cover thousands of kilometers with low vector resolution while other files cover only a small harbor area in high resolution. There is no connection between a polygon representing a real object from one chart file to another. You could say that the data is already broken from the start as the split data limits the computers understanding of the real objects. Creating a seamless map out of scattered chart data is therefore solved graphically (for human perception).

Selecting the correct chart(s) for a particular region of interest is not straight forward. For a particular tile the application by default renders all tile intersecting charts onto a single tile. It is rendered with the highest resolution chart on top. There are also some other rules applied to minimize uneccessary tile generation and rendering efforts - most of them [defined here](https://github.com/hornang/nautograf/blob/1630d56ee4c1b8193759dd271a9d5d3d3158e4bb/src/tilefactory/tilefactory.cpp#L97-L131).

### Internal tile format

Before tiles are rendered the source data in S-57 format is transformed to a Cap'n Proto based format. The internal format is defined in a [Cap'n Proto schema file](src/tilefactory/chartdata.capnp). As with most serialization frameworks for statically typed languages it is based on code generation. The schema defines C++ classes and most importantly the serialization/deserialization code. This provides both a way of caching data to disk and also an internal chart data model used when rendering. On windows you will find the cached tiles under `C:\Users\Username\AppData\Local\nautograf\cache\tiles`. There is no garbage collection so delete the data if it gets to large.

### Rendering

When dealing with many points that needs to be transformed from a 3D model (in this case WGS 84 coordinates) to screen coordinates it makes sense to parallelize it using GPU. OpenGL used to be the cross-platform API to accomplish this, but the future is no longer that clear. Qt has their [own story](https://www.qt.io/blog/graphics-in-qt-6.0-qrhi-qt-quick-qt-quick-3d) on this.

For this application the imperative QPainter method was chosen to draw the actual chart graphics. Google Maps and many similar web applications render vector graphics in real time using WebGL. That is much faster than what can be achieved by an off-screen QPainter. To still achieve sufficient performance the chart data is rendered to a QImage in a separate thread. The last rendered QImage is then drawn to screen possibly stretched - an operation that is cheap and keeps the framerate high. The final pixel-perfect result (non-stretched QImage) does not appear until you stop zooming.

The rest of the user interface is written in QML.

## Background

Electronic nautical charts have historically not been given much attention to by open source or web technology. Nautical navigation is indeed a niche for the average consumer that mostly navigates cities and roads. As a result nautical chart technology is lagging years behind well-known services like Google Maps and OpenStreetMaps.

Access to free nautical chart data is also still quite limited and depends on authorities' willingness to provide map data to the public. The lack of free public nautical data creates a burden for non-profit research and innovation at sea.

The only fully featured open source navigation tool for nautical maps today is [OpenCPN](www.opencpn.org). To resolve the lack of map data OpenCPN vendors in 2013 created the o-charts.org store where you can buy charts in OpenCPN's custom charts format. o-charts receive data from hydrographic offices and adds an end user encryption that gets decrypted in the closed source part of the OpenCPN's plugin [o-charts_pi](https://github.com/bdbcat/o-charts_pi) (previously called oesenc_pi).

Nautograf was created to modernize the archaic world of nautical charts.
