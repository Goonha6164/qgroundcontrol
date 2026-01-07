# Windows GStreamer Enablement (Draft)

This draft shows concrete workflow and code changes without touching originals.
Use this as a reference to apply changes later.

## 1) GitHub Actions (windows_build.yml)

Add a GStreamer install step and set PATH so `gst-*` tools and DLLs are visible.

```yaml
    steps:
      - name: Install GStreamer (MSVC x64)
        run: |
          set "GST_VERSION=1.22.9"
          set "GST_ROOT=C:\gstreamer\1.0\msvc_x86_64"
          curl -L -o gst-runtime.msi https://gstreamer.freedesktop.org/data/pkg/windows/%GST_VERSION%/msvc/gstreamer-1.0-msvc-x86_64-%GST_VERSION%-runtime.msi
          curl -L -o gst-devel.msi   https://gstreamer.freedesktop.org/data/pkg/windows/%GST_VERSION%/msvc/gstreamer-1.0-msvc-x86_64-%GST_VERSION%-devel.msi
          msiexec /i gst-runtime.msi /qn /norestart INSTALLDIR=%GST_ROOT%
          msiexec /i gst-devel.msi /qn /norestart INSTALLDIR=%GST_ROOT%
          echo %GST_ROOT%\bin>> %GITHUB_PATH%
          echo GSTREAMER_1_0_ROOT_MSVC_X86_64=%GST_ROOT%>> %GITHUB_ENV%

      - name: Configure (CMake)
        run: |
          set "Qt5_DIR=%GITHUB_WORKSPACE%\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
          set "GST_ROOT=%GSTREAMER_1_0_ROOT_MSVC_X86_64%"
          cmake -S . -B build -G Ninja ^
            -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
            -DCMAKE_PREFIX_PATH="%Qt5_Dir%;%GST_ROOT%" ^
            -DQGC_GST_STREAMING=ON
```

Notes:
- Pin `GST_VERSION` to a known-good release.
- If your repo uses a different variable name, adjust the `-DQGC_GST_STREAMING=ON`.
- If you already have a global `CMAKE_PREFIX_PATH`, just append `%GST_ROOT%`.

## 2) CMake (linking on Windows)

If QGC already has GStreamer discovery logic, extend it to Windows if needed.
If not, add a minimal block where libraries are linked:

```cmake
if(QGC_GST_STREAMING)
    target_compile_definitions(qgc PRIVATE QGC_GST_STREAMING)
    target_include_directories(qgc PRIVATE "${GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/gstreamer-1.0"
                                   "${GSTREAMER_1_0_ROOT_MSVC_X86_64}/include/glib-2.0"
                                   "${GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib/glib-2.0/include")
    target_link_directories(qgc PRIVATE "${GSTREAMER_1_0_ROOT_MSVC_X86_64}/lib")
    target_link_libraries(qgc PRIVATE gstreamer-1.0 gstbase-1.0 gobject-2.0 glib-2.0)
endif()
```

## 3) Code gating (MAVlinkRelay.cc)

Wrap GStreamer headers and pipeline code so Windows builds succeed when the
SDK is present and the flag is enabled.

```cpp
#if defined(QGC_GST_STREAMING)
#include <gst/gst.h>
#include <glib.h>
#endif
```

```cpp
#if defined(QGC_GST_STREAMING)
struct GstCtx { /* ... */ };
static gboolean gst_bus_cb(GstBus*, GstMessage* msg, gpointer data) { /* ... */ }
static void rtp_thread_main() { /* ... GStreamer pipeline ... */ }
#endif

extern "C" bool relay_start() {
    if (g_running.exchange(true)) return true;
    g_stop.store(false);
    g_udpThread = std::thread(udp_thread_main);
#if defined(QGC_GST_STREAMING)
    g_rtpThread = std::thread(rtp_thread_main);
#endif
    return true;
}

extern "C" void relay_stop() {
    if (!g_running.load()) return;
    g_stop.store(true);
    if (g_udpThread.joinable()) g_udpThread.join();
#if defined(QGC_GST_STREAMING)
    if (g_rtpThread.joinable()) g_rtpThread.join();
#endif
    g_running.store(false);
}
```

## 4) Runtime validation

- `gst-inspect-1.0` and `gst-launch-1.0` should be available on PATH.
- Confirm the pipeline elements used in `MAVlinkRelay.cc` exist.

