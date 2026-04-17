# glad — OpenGL Function Loader

The `glad/` directory must be populated before building.
It is NOT included in this repo because the output depends on your
target OpenGL version and profile.

## How to get glad (takes 60 seconds)

1. Go to:  https://glad.dav1d.de/
2. Set:
   - Language:  C/C++
   - Specification:  OpenGL
   - API gl:  Version 3.3
   - Profile:  Core
   - Extensions:  (none needed for now)
   - Options:  check "Generate a loader"
3. Click "Generate"
4. Download the zip
5. Extract it — you'll get:
       glad/
         include/
           glad/glad.h
           KHR/khrplatform.h
         src/
           glad.c
6. Copy those three files into this project's `glad/` folder so the
   structure matches:
       pbd_sim/
         glad/
           include/glad/glad.h
           include/KHR/khrplatform.h
           src/glad.c

Then proceed with the CMake build steps in BUILD.md.
