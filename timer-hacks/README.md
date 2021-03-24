# Timer Hacks

I'm releasing this, not because you should use it, but for anyone who wants to know how I got my results.

**Don't use this, or do so at your own risk!** `PMCKext2.c` deliberately makes your computer less secure.

To use these tools, I first built `PMCKext2.c` it, installed it as a kernel module, and loaded it.

I have no idea how kernel modules work, how to build it, etc. Sorry. I don't know how I got it working for me, and I can't help you get it working. Please, Apple, make this less confusing. I tried a ton of random crap, and what finally worked was rebooting without knowingly changing anything. `kmutil print-diagnostics` and `kmutil log show` might help. You also may or may not need to give Apple money. Maybe these instructions will work for you: https://github.com/saagarjha/TSOEnabler - they did for me, until I upgraded macOS, and then they didn't.

To test arm64e instructions, I ran `nvram boot-args=-arm64e_preview_abi` as root and reboot.

I build the timer with `sh build.sh`

I run the timer binaries via `python3 bench.py`

Context switches may lead to random results. `bench.py` takes a median of 64 to limit this risk, so I keeping tests short-running and limit multi-tasking (close programs, disconnect from networks, etc).

```
$ python3 bench.py --help               
usage: bench.py [-h] [--init INIT] [--icestorm] [--arm64e] [--unroll UNROLL] [--iters ITERS] [--divide DIVIDE] [--counters {default,all,fast,cycles}] [--zeroes] asm

Process some integers.

positional arguments:
  asm                   Assembly code to test (semicolon separated string)

optional arguments:
  -h, --help            show this help message and exit
  --init INIT           Initialization assembly code (semicolon separated string)
  --icestorm            Test Icestorm instead of the default Firestorm
  --arm64e              Test on arm64e (requires bootarg)
  --unroll UNROLL       Number of copies of the asm in the loop.
  --iters ITERS         Number of iterations to run.
  --divide DIVIDE       Number to divide results by (typically the number of times the instruction being tested is repeated in asm)
  --counters {default,all,fast,cycles}
  --zeroes              Show zero counters

```

```
% python3 bench.py 'nop' --counters=fast --iters=100 --unroll=10000
01 uops_that_retire                1.000204
02 CYCLE                           0.125071
52 INST_ISSUE                      0.000125
53 int_unit_uops                   0.000125
78 unk2_fused_uops                 0.000125

% python3 bench.py 'nop' --counters=fast --iters=100 --unroll=10000 --icestorm
01 uops_that_retire                1.000204
02 CYCLE                           0.250222
52 INST_ISSUE                      0.000101
53 int_unit_uops                   0.000101
78 unk2_fused_uops                 0.0001

% python3 bench.py 'pacga x0, x0, x1' --counters=fast --arm64e --init='mov x0, #0 ; mov x1, #1' 
01 uops_that_retire                1.0204
02 CYCLE                           7.0029
52 INST_ISSUE                      1.01
53 int_unit_uops                   1.01
78 unk2_fused_uops                 1.01

% python3 bench.py 'pacga x0, x0, x1' --arm64e --init='mov x0, #0 ; mov x1, #1' --counters=cycles --ice              
02 CYCLE                           6.0029

% python3 bench.py 'pacga x0, x0, x1' --arm64e --init='mov x0, #0 ; mov x1, #1' --counters=cycles      
02 CYCLE                           7.0029
```
