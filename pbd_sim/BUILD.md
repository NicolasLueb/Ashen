# Build instructions

## Prerequisites

| Tool | Minimum version | Install |
|------|----------------|---------|
| C++ compiler | GCC 11 / Clang 14 / MSVC 2022 | see below |
| CMake | 3.20 | https://cmake.org/download |
| Git | any | https://git-scm.com |
| glad | 3.3 core | see glad/README.md |

Everything else (GLFW, GLM, Dear ImGui) is downloaded automatically
by CMake at configure time — no manual vcpkg/conan needed.

---

## Step 0 — Get glad (required, one-time)

Follow the instructions in `glad/README.md`.
After this step you should have:

    glad/include/glad/glad.h
    glad/include/KHR/khrplatform.h
    glad/src/glad.c

---

## Step 1 — Clone / open the project

    git clone <your-repo-url>   # or just cd into the folder you have
    cd pbd_sim

---

## Step 2 — Configure with CMake

### Linux / macOS

Make sure you have the OpenGL development packages:

    # Ubuntu / Debian
    sudo apt install libgl1-mesa-dev libglu1-mesa-dev xorg-dev

    # macOS — no extra packages needed (OpenGL ships with Xcode CLT)
    xcode-select --install   # if not already done

Then:

    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    # or Release for a fast build:
    cmake -B build -DCMAKE_BUILD_TYPE=Release

### Windows (Visual Studio 2022)

Open a "Developer Command Prompt for VS 2022" and:

    cmake -B build -G "Visual Studio 17 2022" -A x64

Or just open the folder in Visual Studio 2022 — it detects
CMakeLists.txt automatically.

---

## Step 3 — Build

    cmake --build build --parallel

The executable lands at:

    build/pbd_sim          (Linux / macOS)
    build/Debug/pbd_sim.exe    (Windows Debug)
    build/Release/pbd_sim.exe  (Windows Release)

---

## Step 4 — Run

    ./build/pbd_sim        # Linux / macOS
    build\Debug\pbd_sim.exe    # Windows

A 900×700 window opens showing the simulation.
The ImGui panel top-left lets you switch between Step 1 and Step 2,
tune physics parameters, pause, and reset.

---

## Troubleshooting

**CMake can't find OpenGL on Linux**

    sudo apt install libgl-dev

**`glad.h` not found**

You skipped Step 0. See `glad/README.md`.

**FetchContent is slow / fails**

CMake clones GLFW, GLM, and ImGui from GitHub on first configure.
This needs internet access. If you're offline, clone them manually:

    git clone --depth 1 --branch 3.4      https://github.com/glfw/glfw.git     _deps/glfw
    git clone --depth 1 --branch 1.0.1    https://github.com/g-truc/glm.git    _deps/glm
    git clone --depth 1 --branch v1.91.6  https://github.com/ocornut/imgui.git _deps/imgui

Then add to your cmake command:

    -DFETCHCONTENT_SOURCE_DIR_GLFW=_deps/glfw
    -DFETCHCONTENT_SOURCE_DIR_GLM=_deps/glm
    -DFETCHCONTENT_SOURCE_DIR_IMGUI=_deps/imgui

**Window opens then immediately closes**

Usually a shader compile error — check terminal output.
Make sure your GPU supports OpenGL 3.3 (almost all hardware since 2010 does).

**macOS: "App is damaged"**

    xattr -cr ./build/pbd_sim

---

## Next steps (after Step 1 + 2 work)

- Step 3: rope — chain of N particles with N-1 distance constraints
- Step 4: cloth grid — 2D particle grid with structural/shear/bend constraints
- Step 5: collision — sphere and box colliders
- Step 6: XPBD — timestep-independent stiffness
