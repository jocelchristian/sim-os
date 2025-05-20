# sim-os
Operating System GUI Simulator with custom simulation description language.

This is supposed to be a suite of simulations to better grasp how an operating system works under the hood.

These are the planned/currently implemented simulations:
- [x] scheduler
- [ ] virtual memory

# Dependencies
- CMake
- GCC 14+
- OpenGL
- GLFW
- ImGui
- ImPlot
- stb_image.h

# Building
Build instructions are the same as all cmake projects:

```sh
git clone https://github.com/dead-tech/sim-os.git
```

```sh
cd sim-os/
```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

```sh
cmake --build build
```
You will then find the following executables inside the build directory:
- scheduler
- comparator

## scheduler
![image](https://github.com/user-attachments/assets/c4a851fe-b33f-4cb6-b3cc-1e5e56335df0)

This is a scheduler simulator. It expects you to give it a file to a simulation described in the custom simulation description language. See [sim-lang](#sim-lang) for more info.

These are the currently features that the scheduler supports:
- Stepping the simulation one timer tick at a time
- Visualization of the: arrival, ready, waiting queues
- Visualization of running processes (supports multicore)
- Plotting of metrics like: cpu usage, waiting time, turnaround time and throughput
- Different kind of scheduling policy (at compile time though for now)
- Saving result of the simulation and the compare them with [comparator](#comparator)

### comparator
![image](https://github.com/user-attachments/assets/43d0e8ea-32b4-4967-95cc-0769814aeac4)



This is a tool built to compare the result of the simulations produced by the scheduler (for now, planning on making it general purpose). It expects you to pass it to its CLI the simulation results files and it will compare them by graphing histograms.

## sim-lang
This is the language created to ease the description of a simulation, without touching the C++ code. The script will get interpreted just before the simulation is run.

Supported features:
- Specify the number of cores of the CPU
- Specify the max number of processes to spawn
- Specify the max number of events a single process might have
- Specify the max duration of a single event
- Specify the max arrival time for a process from the start of the timer
- Spawn random processes or custom processes
- Restarting the simulation from the beginning

### Examples
For some examples on the syntax of the language checkout [examples](examples).
