# Lightweight Windows Picture Viewer

This document outlines the final technical implementation architecture for the fast, lightweight, and hardware-accelerated picture viewer for Windows.

## Goal Description

Provide a Windows-native picture viewer that prioritizes extreme performance and minimal resource footprint. 

Key features include:
1. Hardware-accelerated image rendering natively scaling bounds via Direct2D.
2. Uncompromised format compatibility including Raw rendering and highly granular zooming control.
3. Seamless intra-folder navigation sorting natively by fast-tracking or explicit EXIF evaluations.
4. Unabstracted low-level OS structures guaranteeing peak boot speeds.

> [!NOTE]
> Taking full advantage of native Windows APIs without intermediate wrappers or heavy UI frameworks remains the philosophical baseline, keeping executable sizes microscopic and performance flawlessly GPU-bound.

## Technology Stack

To achieve maximum performance and the lowest memory footprint, the implementation heavily exploits low-level OS structures:

- **Language**: C++ (Latest natively available in MSVC, C++23/C++26).
- **Core Windowing**: Native Win32 API evaluating standard message loops (`WM_KEYDOWN`, `WM_DROPFILES`, `WM_TIMER`, `WM_MOUSEMOVE`).
- **Rendering Interface**: **Direct2D (D2D)** heavily utilizing the GPU for buttery-smooth rendering and matrix translations.
- **Image Decoding**: **Windows Imaging Component (WIC)** coupled natively to hardware decoding paths utilizing any OS store extensions (`.HEIC`, `.DNG`).
- **Metadata**: **Windows Shell Property System** (`IPropertyStore`). The native metadata extractor fetches system-layer definitions substantially more robustly than WIC headers.
- **Build System**: CMake (compiled via Visual Studio 2026/MSBuild).

## Final Architecture

### 1. Window & Interactivity Processing (Win32)
- Created a highly responsive pure Win32 window class dynamically shifting DPI scaling configurations via `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`.
- **Drag & Drop**: Registers OLE dropping via `DragAcceptFiles` seamlessly routing structural `HDROP` callbacks directly to the navigation array.
- **Title Tracking**: The Window Title dynamically updates to print the physical absolute route of the rendered picture context.
- **UI Toggle**: Pressing the `F` key seamlessly recycles states across:
  1. Standard window with OS title bar.
  2. Borderless Pop-Up mode.
  3. Strict Full-screen spanning bound dimensions natively calculated by the nearest Monitor coordinates.

### 2. Graphics Pipeline (Direct2D + WIC Matrices)
- **Decoding**: For a given path, routes via `IWICImagingFactory` converting generic source blobs into standard 32bpp-PBGRA.
- **Zoom Engineering**:
  - Automatically assesses window deltas applying mathematical restrictions across bounding configurations explicitly scaled via floating variables.
  - Limits max `Ctrl+Scroll` un-bound zoom constraints strictly between limits (`0.25x` and `4.00x`).
  - Implements a smooth `Z` key cycler stepping constraints through: `Fit Height`, `Fit Width`, `Fit Best`, and `100% Native`.
  - **State Persistence**: Zoom scaling values explicitly persist across image loading loops (navigating to the next photo strictly retains your chosen custom zoom magnification).
- **Zoom Panning (Dragging and Translation)**:
  - Intercepts stateful `WM_LBUTTONDOWN` boolean captures.
  - Dynamically recalculates `drawX` and `drawY` constraints mapping mouse drift directly into `m_panX/Y`. Limits pan translation against strict mathematical bounds avoiding drawing beyond window edge voiding.
  - While Zoom configuration inherits between files, active pan bounding explicitly resets to origin configurations (0.0f) during folder navigation.
- **HUD Overlays**: Explicit zoom states fire a Win32 system `SetTimer` locking a transient DirectWrite overlay strictly bound to fade after 1.5 seconds. Display natively overlays alongside stationary `I` key toggled Property details.

### 3. Folder Navigation & Traversal Optimizations
- **Safe Directory Isolation**: Sweeps through filesystem loops using deep internal `error_code` iterators (`fs::directory_options::skip_permission_denied`) ignoring protected OS elements entirely without failing or throwing abortive `std::filesystem::error`.
- **Format Parity**: Architecture dynamically iterates and boots `IWICImagingFactory::CreateComponentEnumerator(WICDecoder)` at startup. Constructs an internal cache mapping every file extension universally supported by your installed native OS Raw decoders. This fundamentally guarantees flawless access to specialized formats like `.HEIC`, `.DNG`, or localized `.CR2` variants strictly based on native Windows extension mapping.
- **High-Efficiency Pre-loads**: Iteration relies defaulting entirely to absolute Path configurations preventing synchronous blocking.
- **Explicit EXIF Mapping (`S` Key)**: Deep hardware `IPropertyStore.GetValue` execution is mapped strictly to user polling. On execution, safely invalidates an immediate UI loop rendering an active "Please Wait" screen, re-sorting all values by literal `PKEY_Photo_DateTaken`, prior to reinstating active rendering configuration safely on the last-tracked visual bounds.
- **Shortcut Controls**:
  - `Left/Right` keys assess current visual bounding constraints, translating either directly into logical file navigation or panning active translation vectors depending entirely on the active bounds exceeding the underlying window scale.
  - `Page Up / Page Down` circumvent panning translating exclusively to folder structures.
  - `Home / End` arrays explicitly load immediate vectors mapped entirely to zero-order or endpoint maximums.
  - `L Key` translates directly outward via an external `/select` binding onto native `explorer.exe`, popping the OS file explorer strictly overlapping the underlying photo structure path on physical disk.
- **Background Prefetching**:
  - Implements a single background worker thread (`std::thread`) to seamlessly pre-decode `IWICBitmap` references asynchronously into an in-memory cache map.
  - Dynamically evaluates a continuous sliding window covering 10 total images (-3 to +6 from the center viewpoint). Window calculations explicitly wrap around logic-bounds scaling effectively across endpoints.
  - Decodes full arrays cleanly using `WICBitmapCacheOnLoad` converting raw blobs exclusively off the UI thread and maintaining lockless synchrony ensuring ultra-fast D2D drawing and virtually zero navigation latency.
