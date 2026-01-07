# Windows GStreamer Setup (QGC)

This document outlines how to enable GStreamer on Windows so that
`MAVlinkRelay.cc` can build and run with the GStreamer pipeline.

## 1) Install GStreamer SDK on Windows CI

- Install both Runtime and Development MSI packages for MSVC x86_64.
- Suggested install root: `C:\gstreamer\1.0\msvc_x86_64`.

## 2) Set environment variables and PATH

- Add to PATH:
  - `C:\gstreamer\1.0\msvc_x86_64\bin`
- Provide a hint for CMake to find the SDK:
  - `GSTREAMER_1_0_ROOT_MSVC_X86_64=C:\gstreamer\1.0\msvc_x86_64`
  - or pass `-DCMAKE_PREFIX_PATH=C:\gstreamer\1.0\msvc_x86_64`

## 3) CMake configuration (Windows)

- Enable the GStreamer feature flag used by QGC.
  - Example: `-DQGC_GST_STREAMING=ON`
- Ensure CMake can find GStreamer:
  - `find_package(GStreamer REQUIRED)` if supported in the tree,
    otherwise add include/lib paths manually.
- Link required libraries:
  - `gstreamer-1.0`
  - `gstbase-1.0`
  - `gobject-2.0`
  - `glib-2.0`
- Optional (depending on pipeline elements):
  - `gstvideo-1.0`
  - `gstapp-1.0`

## 4) Code gating in MAVlinkRelay.cc

- Wrap GStreamer headers and implementation with:
  - `#if defined(QGC_GST_STREAMING)`
- Only create the GStreamer thread if the flag is enabled.

## 5) Runtime validation

- Verify that `gst-inspect-1.0` and `gst-launch-1.0` are on PATH.
- Confirm the pipeline elements used by the relay exist on Windows.
- Run the app and verify the UDP + GStreamer relay actually works.

