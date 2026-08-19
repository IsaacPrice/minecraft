# Third-party components

This repository vendors the following libraries. They are not covered by the
project's own [LICENSE](LICENSE) and remain under the terms below.

| Component | Location | Licence |
| --- | --- | --- |
| OpenGL Mathematics (GLM) | `include/glm/` | The Happy Bunny License or MIT — see `include/glm/copying.txt` |
| GLFW 3.4 | `include/GLFW/`, `lib/` | zlib/libpng |
| glad 0.1.36 (generated GL loader) | `include/glad/`, `include/KHR/`, `src/glad.c` | MIT (generated output; Khronos headers under the Khronos free-use licence) |
| stb_image | `src/headers/stb_image.h` | MIT or public domain (Unlicense), at your option |
| FastNoise | `src/headers/FastNoise.hpp`, `src/FastNoise.cpp` | MIT — Copyright (c) 2017 Jordan Peck |

Full licence text for each is kept in the corresponding source file header, or
in `include/glm/copying.txt` for GLM.

## Textures

`content/terrain.png` is a Minecraft block atlas. Minecraft is a trademark of
Mojang Studios, and the texture is not covered by this project's licence. It is
included here for the sake of a runnable demo; replace it before redistributing
this project.
