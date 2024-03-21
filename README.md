![Nautograf](qml/graphics/title.svg)

## Introduction

Nautograf is an experimental viewer for marine vector charts. It is inspired by [OpenCPN](https://www.opencpn.org) which is the only nautical chart software that is open source and runs with offline chart data. The aim of Nautograf is to offer a user experience similar to OpenCPN but with better zooming and panning, better rendering quality, and a more modern codebase.

Nautograf is designed for vector charts. This kind of data is not open to the public for all areas. Therefore Nautograf currently only supports _oesu_ charts from [o-charts.org](https://www.o-charts.org) where you can buy cheap charts for most areas. Support for S-57 charts, such as those provided by https://www.noaa.gov/ may be added in the future.

## Installing

Releases are available from The Microsoft Store and the Snap Store:

[<img src="https://get.microsoft.com/images/en-us%20dark.svg" width="165"/>](https://apps.microsoft.com/detail/Nautograf/9NP97HF6LW08)
[![Get it from the snap store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/nautograf)

The version in the Snap Store only supports unencrypted charts. Binaries are also available as artifacts in the releases page at GitHub.

## Development status

![CI status badge](https://github.com/hornang/nautograf/actions/workflows/ci.yml/badge.svg)

Nautograf is a spare time project so progress is varying and non-deterministic. It is likely that it will never come up to a point of being sufficient for navigation. The current feature set is limited and **not sufficient for nautical navigation**. 

Nonetheless, the current implementation can display the most prominent S-57 features such as land areas, depth areas and depth numbers. Chart navigation works by using conventional _zoom to cursor_ or pinch zoom on touch capable devices.

![Screenshot of chart](https://store-images.s-microsoft.com/image/apps.39713.13722934716828675.f9abff29-8e4b-4550-8d49-482c93232558.849720b8-bbce-4b30-82d9-4b9f5afa86e4)

Nautograf currently only supports _oesu_ charts from [o-charts.org](https://www.o-charts.org). Nautograf uses OpenCPN's decryption engine _oexserverd_ to decrypt the charts on the fly. You can read more about that in the [oesenc-export](https://github.com/hornang/oesenc-export) repository.

## Design

Most online map viewers are based on [tiled web maps](https://en.wikipedia.org/wiki/Tiled_web_map). Tiling is an important feature to support smooth zooming and panning for the user. Online map viewers usually renders tiles that are already generated and hosted from a server. Nautograf is also tile based, but has to create those tiles internally because the chart data is provided by the user.

Because tile creation can take some time it runs in multiple threads. Each tile being generated is put in a queue and waits for the next available thread. Generated tile data is cached to disk. This means that each time you navigate to a *new area* it will take some time before the area shows. Keeping this delay as low as possible is a key factor to improve usability of the application.

### Internal tile format

Before tiles are rendered the source data format is transformed to a [Cap'n Proto](https://capnproto.org/) based format. The internal format is defined in a [schema file](src/tilefactory/chartdata.capnp). The schema defines C++ classes and serialization/deserialization code. This provides both a way of caching data to disk and also an internal data model used when rendering. On Windows you will find the cached tiles under `C:\Users\Username\AppData\Local\nautograf\cache\tiles`. There is no garbage collection so delete the data if it gets too large.

### Rendering

At the start of the project rendered was done using [QPainter](https://doc.qt.io/qt-6/qpainter.html). This provided good quality, but was too slow for a proper pan/zoom experience. Therefore the render code is now based on [The Qt Quick Scene Graph](https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html). This approach is very similar to the approach used by WebGL based map applications.

## Background

Historically, electronic nautical charts have not received significant attention from open source or web technology communities. Nautical navigation, being a niche area compared to urban and road navigation, has led to a lag in technological advancements in nautical charting when compared to popular services like Google Maps and OpenStreetMap. 

Access to free nautical chart data remains limited, often relying on authorities' willingness to share map data with the public. This scarcity of freely available nautical data poses challenges for non-profit research and innovation within this area.

Currently, the primary open source navigation tool for nautical maps is [OpenCPN](https://www.opencpn.org). To address the lack of readily available vector chart data, the creators of OpenCPN established the o-charts.org store in 2013. This store offers charts in OpenCPN's proprietary format, sourced from hydrographic offices. The o-charts incorporate end-user encryption that is decrypted within the closed-source component of OpenCPN's plugin, previously known as oesenc_pi and now as [o-charts_pi](https://github.com/bdbcat/o-charts_pi).
