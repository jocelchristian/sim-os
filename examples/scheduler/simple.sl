spawn_process("Game", 0, 0, [(Io, 5), (Cpu, 1), (Io, 5), (Cpu, 2)])
spawn_process("Compiler", 1, 4, [(Cpu, 20), (Io, 1), (Cpu, 10)])
spawn_process("Text Editor", 2, 0, [(Cpu, 4), (Io, 1), (Io, 2), (Cpu, 1), (Io, 2), (Cpu, 1)])
spawn_process("Web browser", 3, 7, [(Io, 5), (Cpu, 10), (Io, 2), (Io, 3)])
