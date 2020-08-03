Requires glew 2.1.0+, glfw 3.3+, freeglut 3.0.0, CUDA 9.0+ and SFML 2.5.1+ to be installed and locate in corresponding property sheets. (Yes, I know, manual preparation is evil, but this was a part of code for a university task)

Simulates two-dimesional molecular dynamics with Lennard-Jones potential using Verlet method.
Consists of Source.cpp, which is SFML CPU version and SourceGPU.cpp which speaks for itself. GPU version is notably faster, but very hard to read, since executable is scattered between compute shaders.

For purposes of quick demonstration there is an executable in Bin64 folder, which has already been set up to simulate dense structure of 1024 particles. Tested on Windows 10 x64, requires a decent GPU with OpenGL 4.3+ support (99.9% of modern GPUs).
