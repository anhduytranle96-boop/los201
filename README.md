# LOS201 - Embedded Linux & System Programming

Bài tập thực hành môn Hệ thống nhúng (LOS201) - FPT University.
SV: Trần Lê Anh Duy

## Structure
- LAB1: Cross-compile Kernel, QEMU boot
- LAB2: Custom RootFS, BusyBox init
- LAB3: Debug, Memory Leak, Profiling & Real-Time Latency
- ASM1 / ASM2: Assignment drivers & apps

## LAB3 Overview & Commands

### 1. Remote Debugging (buggy_app)
Biên dịch tĩnh và debug qua QEMU:
```bash
arm-linux-gnueabihf-gcc -g -O0 -static -o buggy_app buggy_app.c
qemu-arm -g 1234 ./buggy_app &
gdb-multiarch buggy_app -ex "target remote :1234"
```

### 2. Memory & Profiling (perf_target)
Phân tích CPU Hotspot & System call:
```bash
gcc -g -O1 -o perf_target perf_target.c -lm
strace -c -o logs/strace_summary.txt ./perf_target
sudo perf record -g -o logs/perf.data ./perf_target
sudo perf report -i logs/perf.data --stdio > logs/perf_report.txt
```

### 3. RT Latency (cyclictest)
Đo latency và xuất chart:
```bash
sudo cyclictest --mlockall --threads=4 --priority=80 --interval=1000 --distance=500 --loops=10000 --histogram=200 --histfile=logs/hist_standard.txt
gnuplot logs/plot_latency.gnuplot
```
