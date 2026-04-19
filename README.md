# picv - Lightweight Windows Picture Viewer

`picv` is an ultra-fast, extremely lightweight native C++ picture viewer engineered for Windows. Built directly on top of raw Win32 APIs, Direct2D, and the Windows Imaging Component (WIC), it provides absolute zero-overhead GPU-accelerated rendering and instantaneous load times explicitly optimized for massive media folders and heavy structural formats.

## Navigation & Controls

`picv` features heavy UX considerations to ensure traversing thousands of files is intuitive and frictionless.

### Folder Navigation
By default, placing a photo into `picv` will instantly bind and alphabetically sort every adjacent photo within that directory.

- **Mouse Scroll Wheel**: Navigate to the Next / Previous image.
- **Left / Right Arrow Keys**: Navigate to the Next / Previous image. *(Note: If the current image is heavily magnified and bleeds off-screen, these keys will instead physically pan the image)*.
- **Page Up / Page Down**: Strictly force sequential navigation to the Next / Previous image regardless of how deeply zoomed in you are.
- **Home / End**: Jump instantaneously to the absolute first or last image in the current folder's sorted array.
- **S Key**: Halt dynamic loading and explicitly execute a deep-scan hardware sort prioritizing the exact `EXIF Capture Date` of all images in the folder.

### Zooming & Panning
A dedicated view-matrix controls precise magnification without blurring or performance drops. Zoom configurations seamlessly persist as you navigate dynamically between new images.

- **Z Key**: Instantly cycles the zoom matrix bounds through four view profiles:
  1. `Fit Best` (scales down limits to fit the window safely).
  2. `Fit Height`
  3. `Fit Width`
  4. `100% Native View`
- **Ctrl + Mouse Scroll**: Detach from pre-set boundaries and smoothly custom-magnify the active image anywhere from `25%` to `400%`.
- **Left-Click + Drag**: If an image is scaled beyond the physical viewport of your screen, safely grab the image using the left mouse button and arbitrarily throw/pan your view around.
- **Up / Down / Left / Right Arrow Keys**: Micro-step panning adjustments cleanly around the enlarged image by 50-pixel vectors.

### Window States & Overlays
No heavy menus or settings logic exist. UI is strictly hotkey-bound.

- **F Key**: Successively toggles your window container format:
  1. Standard Windows-tracked layout.
  2. Pure chromeless borderless pop-up.
  3. Strict Desktop-spanning Full-screen.
- **I Key**: Toggles a live metadata heads-up overlay decoding and printing the associated `Camera Model`, true `Exposure`, `ISO`, and internal `Dimensions` pulled directly via the native OS `IPropertyStore`.
- **L Key**: Implements an instantaneous breakout jumping natively into `explorer.exe`, seamlessly highlighting the photo identically where it resides on your physical hard drive structure.
- **Drag-And-Drop**: Flawlessly intercept any incoming Windows Drag interaction, instantly loading dropped imagery mid-run.

## Build Instructions

`picv` is developed entirely using Standard C++ targeting native OS features and requires the Microsoft Visual Studio CMake framework.

### Prerequisites
1. Installed toolchain via [Visual Studio 2026](https://visualstudio.microsoft.com/vs/) (or newer).
2. Within the installer, select the **"Desktop development with C++"** core workload.
3. On the right-side details panel, ensure the **Windows 11 SDK** and **C++ CMake tools for Windows** packages are actively checked.

### Compiling from Source
Once the MSVC toolset is successfully configured, building the `.exe` requires just two commands:

1. Launch your **x64 Native Tools Command Prompt for VS 2026** (or activate the `vcvars64.bat` script).
2. Navigate terminal to the root of this cloned repository.
3. Execute the CMake build process:
   ```cmd
   cmake -B build
   cmake --build build --config Release
   ```
4. The GPU-accelerated application will be freshly dumped at `build\Release\picv.exe`.
