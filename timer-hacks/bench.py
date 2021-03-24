import subprocess
import struct
import os
import sys
import hashlib
import json

import argparse

parser = argparse.ArgumentParser(description='Process some integers.')

parser.add_argument('asm', help='Assembly code to test (semicolon separated string)')

parser.add_argument('--init', help='Initialization assembly code (semicolon separated string)')
parser.add_argument('--icestorm', action='store_true', help='Test Icestorm instead of the default Firestorm')
parser.add_argument('--arm64e', action='store_true', help='Test on arm64e (requires bootarg)')

parser.add_argument('--unroll', type=int, default=100, help='Number of copies of the asm in the loop.')
parser.add_argument('--iters', type=int, default=100, help='Number of iterations to run.')
parser.add_argument('--divide', type=int, default=1, help='Number to divide results by (typically the number of times the instruction being tested is repeated in asm)')
parser.add_argument('--counters', default='default', choices=['default', 'all', 'fast', 'cycles'])
parser.add_argument('--zeroes', action='store_true', help='Show zero counters')

args = parser.parse_args()

DEFAULT_CONFIG = [
  0x54535202, 0x57560155,
  0x5b5a5958, 0x7e7d7c78,
  0xe981807f, 0x00efeeed,
]

if args.counters == 'fast':
	config = [
	  0x54535202, 0x78000155
	]
elif args.counters == 'cycles':
	config = [
		2 * 0x01010101, 2 * 0x01010101
	]
elif args.counters == 'all':
	config = []
	for i in range(1, 256):
		config.append(i * 0x01010101)
		config.append(i * 0x01010101)
else:
	config = DEFAULT_CONFIG

all_counters = set()
for i in config:
	for j in range(0, 32, 8):
		all_counters.add((i >> j) & 0xFF)

if 0 in all_counters:
	all_counters.remove(0)
all_counters = ['%02x' % i for i in sorted(all_counters)]
CYCLES = '02'
INST_ISSUE = '52'

if args.icestorm:
	SUFFIX = '-icestorm'
else:
	SUFFIX = '-firestorm'

if args.arm64e:
	SUFFIX += '-arm64e'

def assemble(code):
	if not code:
		return b''
	with open('tmp1.s', 'w') as f:
		f.write(code.replace(';', '\n'))
	if subprocess.call(['clang', '-c', 'tmp1.s', '-march=armv8.5-a+sha3+fp16fml']):
		exit(1)

	txt = subprocess.check_output(['otool', '-t', 'tmp1.o']).decode('ascii')
	u32s = []
	for line in txt.strip().split('\n')[2:]:
		for u32 in line.split()[1:]:
			u32s.append(struct.pack('<I', int(u32, 16)))
	os.unlink('tmp1.s')
	os.unlink('tmp1.o')
	return b''.join(u32s)


COUNTER_NAMES = {
	0x02: 'CYCLE',
	0x0d: 'MMU_MISS',
	0x52: 'INST_ISSUE',
	0x6c: 'INTERRUPT_PENDING',
	0x70: 'DISPATCH_STALL',
	0x75: 'SCHEDULER_REWIND',
	0x76: 'SCHEDULER_STALL',
	0x84: 'PIPELINE_REDIRECT',
	0x8c: 'INST_ALL',
	0x8d: 'INST_BRANCH',
	0x8e: 'INST_FUNCTION_CALLS',
	0x8f: 'INST_FUNCTION_RETURNS',
	0x97: 'INST_INTEGER',
	0x9a: 'INST_NEON_OR_FP',
	0x9b: 'INST_LDST',
	0x9c: 'INST_BARRIER',
	0xb3: 'ATOMIC_OR_EXCLUSIVE_SUCCESS',
	0xb4: 'ATOMIC_OR_EXCLUSIVE_FAIL',
	0xbf: 'DCACHE_LOAD_MISS',
	0xc0: 'DCACHE_STORE_MISS',
	0xc1: 'DTLB_MISS',
	0xc4: 'MEMORY_ORDER_VIOLATION',
	0xcb: 'BRANCH_MISPREDICT',
	0xd3: 'ICACHE_MISS',
	0xd4: 'ITLB_MISS',
	0xde: 'INST_FETCH_RESTART',

	0x01: 'uops_that_retire',

	0x53: 'int_unit_uops',
	0x54: 'fp_unit_uops',
	0x55: 'ldst_unit_uops',


	0x56: 'int_unk1_uops',
	0x57: 'fp_unk1_uops',
	0x58: 'ldst_unk1_uops',

	0x78: 'unk2_fused_uops',

	0x7C: 'int_unk3_unfused_uops',
	0x7E: 'fp_unk3_unfused_uops',
	0x7D: 'ldst_unk3_unfused_uops',

	0x7F: 'int_unk4_deps',
	0x81: 'fp_unk4_deps',
	0x80: 'ldst_unk4_deps',


	0xe9: 'int_unk5',
	0xee: 'fp_unk6',
	0xed: 'ldst_unk7',
	0xef: 'int_writes_unk8',

	0x59: 'unk9_int_count',
    0x5A: 'unk9_ldst_count',
    0x5B: 'unk9_fp_count',

    0x4C: 'opt_shift_ext',

    0x90: 'inst_branch_taken',
    0x94: 'inst_bcc',
    0x95: 'inst_int_load',
    0x96: 'inst_int_store',
    0x98: 'inst_fp_load',
    0x99: 'inst_fp_store',
}

def experiment(code, init_code='', unroll_count=100, iter_count=100, config=DEFAULT_CONFIG):
	key = repr((code, init_code, unroll_count, iter_count))

	with open('tmp.code.bin', 'wb') as f:
		f.write(assemble(code))

	with open('tmp.init_code.bin', 'wb') as f:
		f.write(assemble(init_code))

	with open('tmp.config.bin', 'wb') as f:
		for i in config:
			f.write(struct.pack('<I', i))

	suffix = ''
	suffix += SUFFIX
	cmd = "./timer%s tmp.code.bin tmp.init_code.bin %d %d tmp.config.bin" % (suffix, unroll_count, iter_count)

	for retry in range(20):
		#print(cmd)
		r = subprocess.check_output(cmd, shell=True).decode('utf-8')
		
		r = r.replace('SIGILL', '')
		r = r.replace('context switch', '')

		if 'SIGILL' in r and r.count('SIGILL') < 9:
			r = r.replace('SIGILL', '')

		try:
			j = json.loads(r)
		except json.decoder.JSONDecodeError:
			continue

		return j

	print(r)
	exit(1)

def median(j, counter):
	counter = counter.lower()
	if counter in j:
		values = j[counter][:]
		values.sort()
		return values[len(values) // 2]
	return 0


counters = experiment(args.asm, unroll_count=args.unroll, iter_count=args.iters, init_code=args.init or '', config=config)

for i in all_counters:
	value = median(counters, i)
	if not args.zeroes and value == 0:
		continue
	value = value / float(args.unroll * args.iters * args.divide)

	l = len(str(args.unroll * args.iters * args.divide - 1))
	fmt = '%' + str(l+6) + '.' + str(l) + 'f'

	print(i, COUNTER_NAMES.get(int(i, 16), '').ljust(27), (fmt % value).rstrip('0').rstrip('.'))
