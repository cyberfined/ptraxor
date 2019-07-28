# ptraxor
Tool for bypassing ptrace(ptrace_traceme) anti-debugging trick.

# Usage
```bash
ptraxor -m <tracer> [args] -s <traced> [args]
```
Execute traced, attach to it, find ptrace_traceme syscall, set eax=0, detach from traced, execute tracer with last argument equal to pid of traced.

# Example
```bash
ptraxor -m strace -p -s somep
```
Bypass ptrace_tracme in somep and execute strace -p <pid_of_traced>.

# Bugs
When ptraxor detach from traced it send SIGSTOP to it, so to execute some tracers you need to send SIGCONT to traced.

# Build
1. git clone https://github.com/cyberfined/ptraxor.git && cd ptraxor
2. make
