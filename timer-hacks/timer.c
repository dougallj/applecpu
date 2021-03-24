#include <assert.h>
#include <dlfcn.h>
#include <getopt.h>
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <ptrauth.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <fcntl.h>

#define OUTER_ITS 64

#define AMX_START()                                                            \
  __asm__ volatile(                                                            \
      "nop \r\n nop \r\n nop \r\n .word (0x201000 | (17 << 5) | 0)" ::         \
          : "memory")
#define AMX_STOP()                                                             \
  __asm__ volatile(                                                            \
      "nop \r\n nop \r\n nop \r\n .word (0x201000 | (17 << 5) | 1)" ::         \
          : "memory")

#define SREG_PMCR0 "S3_1_c15_c0_0"
#define SREG_PMCR1 "S3_1_c15_c1_0"
#define SREG_PMESR0 "S3_1_c15_c5_0"
#define SREG_PMESR1 "S3_1_c15_c6_0"

#define SREG_PMC0 "S3_2_c15_c0_0"
#define SREG_PMC1 "S3_2_c15_c1_0"
#define SREG_PMC2 "S3_2_c15_c2_0"
#define SREG_PMC3 "S3_2_c15_c3_0"
#define SREG_PMC4 "S3_2_c15_c4_0"
#define SREG_PMC5 "S3_2_c15_c5_0"
#define SREG_PMC6 "S3_2_c15_c6_0"
#define SREG_PMC7 "S3_2_c15_c7_0"
#define SREG_PMC8 "S3_2_c15_c9_0"
#define SREG_PMC9 "S3_2_c15_c10_0"

#define SREG_WRITE(SR, V)                                                      \
  __asm__ volatile("msr " SR ", %0 \r\n isb \r\n" : : "r"((uint64_t)V))
#define SREG_READ(SR)                                                          \
  ({                                                                           \
    uint64_t VAL = 0;                                                          \
    __asm__ volatile("isb \r\n mrs %0, " SR " \r\n isb \r\n" : "=r"(VAL));     \
    VAL;                                                                       \
  })

struct file_data {
  size_t size;
  uint32_t *data;
};

static struct file_data load_file(char *filename) {
  struct file_data result = { 0, NULL };
  int fd = open(filename, O_RDONLY);
  if (fd >= 0) {
    size_t length = lseek(fd, 0, SEEK_END);
    if (length) {
      result.data = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
      result.size = length;
    }
    close(fd);
  }
  return result;
}

static void set_sysctl(const char *name, uint64_t v) {
  if (sysctlbyname(name, NULL, 0, &v, sizeof v)) {
    printf("set_sysctl: sysctlbyname failed\n");
    exit(1);
  }
}

#define COUNTERS_COUNT 10

static uint64_t g_counters0[COUNTERS_COUNT];
static uint64_t g_counters1[COUNTERS_COUNT];

static void make_routine(uint32_t *ibuf, int unroll_count, int iter_count,
                         uint32_t *body, size_t body_icount, uint32_t *init,
                         size_t init_icount) {
  pthread_jit_write_protect_np(0);
  int o = 0;

  // prologue
  ibuf[o++] = 0xa9b47bfd; // stp x29, x30, [sp, #-0xC0]!
  ibuf[o++] = 0xa9013ff0; // stp x16, x15, [sp, #0x10]
  ibuf[o++] = 0xa90247f2; // stp x18, x17, [sp, #0x20]
  ibuf[o++] = 0xa9034ff4; // stp x20, x19, [sp, #0x30]
  ibuf[o++] = 0xa90457f6; // stp x22, x21, [sp, #0x40]
  ibuf[o++] = 0xa9055ff8; // stp x24, x23, [sp, #0x50]
  ibuf[o++] = 0xa90667fa; // stp x26, x25, [sp, #0x60]
  ibuf[o++] = 0xa9076ffc; // stp x28, x27, [sp, #0x70]
  ibuf[o++] = 0x6d083bef; // stp d15, d14, [sp, #0x80]
  ibuf[o++] = 0x6d0933ed; // stp d13, d12, [sp, #0x90]
  ibuf[o++] = 0x6d0a2beb; // stp d11, d10, [sp, #0xA0]
  ibuf[o++] = 0x6d0b23e9; // stp d9,  d8,  [sp, #0xB0]

  // prep
  ibuf[o++] = 0x2a0003fc; // mov w28, w0
  ibuf[o++] = 0xaa0103fb; // mov x27, x1
  ibuf[o++] = 0xaa0203fd; // mov x29, x2

  for (size_t i = 0; i < init_icount; i++) {
    ibuf[o++] = init[i];
  }

  // enable
  ibuf[o++] = 0xd519f01b; // msr S3_1_C15_C0_0, x27
  ibuf[o++] = 0xd5033fdf; // isb

  int start = o;

  for (int unroll = 0; unroll < unroll_count; unroll++) {
    for (size_t i = 0; i < body_icount; i++) {
      ibuf[o++] = body[i];
    }
  }

  if (iter_count > 1) {
    // loop back to top
    ibuf[o++] = 0xd100079c; // sub x28, x28, #1
    int off = start - o;
    assert(off < 0 && off > -0x40000);
    ibuf[o++] = 0xb500001c | ((off & 0x7ffff) << 5); // cbnz x28
  }

  // disable
  ibuf[o++] = 0xd519f01d; // msr S3_1_C15_C0_0, x29
  ibuf[o++] = 0xd5033fdf; // isb

  // epilogue
  ibuf[o++] = 0xa9413ff0; // ldp x16, x15, [sp, #0x10]
  ibuf[o++] = 0xa94247f2; // ldp x18, x17, [sp, #0x20]
  ibuf[o++] = 0xa9434ff4; // ldp x20, x19, [sp, #0x30]
  ibuf[o++] = 0xa94457f6; // ldp x22, x21, [sp, #0x40]
  ibuf[o++] = 0xa9455ff8; // ldp x24, x23, [sp, #0x50]
  ibuf[o++] = 0xa94667fa; // ldp x26, x25, [sp, #0x60]
  ibuf[o++] = 0xa9476ffc; // ldp x28, x27, [sp, #0x70]
  ibuf[o++] = 0x6d483bef; // ldp d15, d14, [sp, #0x80]
  ibuf[o++] = 0x6d4933ed; // ldp d13, d12, [sp, #0x90]
  ibuf[o++] = 0x6d4a2beb; // ldp d11, d10, [sp, #0xA0]
  ibuf[o++] = 0x6d4b23e9; // ldp d9,  d8,  [sp, #0xB0]
  ibuf[o++] = 0xa8cc7bfd; // ldp x29, x30, [sp], #0xC0
  ibuf[o++] = 0xd65f03c0; // ret

#if 0
  // clang timer.dump.s -c && objdump -d timer.dump.o
  FILE *f = fopen("timer.dump.s", "wb");
  if (f) {
    for (int i = 0; i < o; i++) {
      fprintf(f, ".word 0x%X\n", ibuf[i]);
    }
    fclose(f);
  }
#endif

  pthread_jit_write_protect_np(1);
  sys_icache_invalidate(ibuf, o * 4);
}

static jmp_buf retry;
static void sighandler() {
  printf("SIGILL\n");
  longjmp(retry, 1);
}

static char memory[4 * 1024 * 1024];

static int get_current_core() { return SREG_READ("TPIDRRO_EL0") & 7; }

int main(int argc, char **argv) {
  signal(SIGILL, sighandler);

  assert(argc == 6);

#ifdef ICESTORM
  pthread_set_qos_class_self_np(QOS_CLASS_BACKGROUND, 0);
#else
  pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

  struct file_data configs_file = load_file(argv[5]);
  uint32_t *configs = configs_file.data;
  size_t configs_size = configs_file.size;

  struct file_data test_code_file = load_file(argv[1]);
  uint32_t *test_code = test_code_file.data;
  size_t test_code_size = test_code_file.size;

  struct file_data init_code_file = load_file(argv[2]);
  uint32_t *init_code = init_code_file.data;
  size_t init_code_size = init_code_file.size;

  void *mapping =
      mmap(NULL, 0x400000 + ((test_code_size + init_code_size) & ~0xFFFF),
           PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANON | MAP_PRIVATE | MAP_JIT,
           -1, 0);
  uint32_t *ibuf = (uint32_t *)mapping;

  size_t unroll_count = atoi(argv[3]);
  size_t iter_count = atoi(argv[4]);

  make_routine(ibuf, unroll_count, iter_count, test_code, test_code_size / 4,
               init_code, init_code_size / 4);

  uint64_t (*routine)(uint64_t, uint64_t, uint64_t, void *, void *, void *,
                      void *) =
      ptrauth_sign_unauthenticated((void *)ibuf, ptrauth_key_function_pointer,
                                   0);

  uint64_t sum_diffs[0x100];
  uint64_t counts[0x100];
  uint64_t results[0x100][OUTER_ITS];

  for (int i = 0; i < 256; i++) {
    sum_diffs[i] = 0;
    counts[i] = 0;
  }

  for (int outer_i = 0; outer_i < OUTER_ITS; outer_i++) {
    for (size_t config_index = 0; config_index < configs_size / 4;
         config_index += 2) {

      uint64_t end0, end1;

      {
      Lretry:
        AMX_START();

        // in case we migrate cores and lose control of PMCR
        setjmp(retry);

        int start_core = get_current_core();

#define ENABLE 0x3003400ff4ff
#define DISABLE 0x3000400ff403
        set_sysctl("kern.pmcr0", 0x3003400ff4ff);

        SREG_WRITE(SREG_PMESR0, configs[config_index]);
        SREG_WRITE(SREG_PMESR1, configs[config_index + 1]);
        SREG_WRITE(SREG_PMCR1, 0x3000003ff00);
        SREG_WRITE(SREG_PMCR0, DISABLE);

        g_counters0[0] = SREG_READ(SREG_PMC0);
        g_counters0[1] = SREG_READ(SREG_PMC1);

        SREG_WRITE(SREG_PMC2, 0);
        SREG_WRITE(SREG_PMC3, 0);
        SREG_WRITE(SREG_PMC4, 0);
        SREG_WRITE(SREG_PMC5, 0);
        SREG_WRITE(SREG_PMC6, 0);
        SREG_WRITE(SREG_PMC7, 0);
        SREG_WRITE(SREG_PMC8, 0);
        SREG_WRITE(SREG_PMC9, 0);

        routine(iter_count, ENABLE, DISABLE, &memory, &memory, &memory,
                &memory);

        g_counters1[0] = SREG_READ(SREG_PMC0);
        g_counters1[1] = SREG_READ(SREG_PMC1);
        g_counters1[2] = SREG_READ(SREG_PMC2);
        g_counters1[3] = SREG_READ(SREG_PMC3);
        g_counters1[4] = SREG_READ(SREG_PMC4);
        g_counters1[5] = SREG_READ(SREG_PMC5);
        g_counters1[6] = SREG_READ(SREG_PMC6);
        g_counters1[7] = SREG_READ(SREG_PMC7);
        g_counters1[8] = SREG_READ(SREG_PMC8);
        g_counters1[9] = SREG_READ(SREG_PMC9);

        end0 = SREG_READ(SREG_PMESR0);
        end1 = SREG_READ(SREG_PMESR1);
        SREG_WRITE(SREG_PMESR0, 0);
        SREG_WRITE(SREG_PMESR1, 0);

        int end_core = get_current_core();

        AMX_STOP();

        if (start_core != end_core || end0 != configs[config_index] ||
            end1 != configs[config_index + 1]) {
          // yes, obviously this is a terrible hack that doesn't really fix
          // things.
          printf("context switch\n");
          goto Lretry;
        }

#ifdef ICESTORM
        if (start_core >= 4) {
          goto Lretry;
        }
#else
        if (start_core < 4) {
          goto Lretry;
        }
#endif
      }

      if (configs[config_index] == configs[config_index + 1]) {
        int mask = 0;
        int good = 1;
        uint64_t value = 0;
        for (int v = 2; v < COUNTERS_COUNT; v++) {
          if (g_counters1[v]) {
            if (value && g_counters1[v] != value) {
              good = 0;
            } else {
              value = g_counters1[v];
            }
            mask |= (1 << v);
          }
        }

        int event = configs[config_index] & 0xFF;
        if (counts[event] >= OUTER_ITS) {
          printf("error\n");
          exit(1);
        }
        results[event][counts[event]++] = value;
        sum_diffs[event] += value;

      } else {
        uint64_t config = (((uint64_t)configs[config_index + 1]) << 32) |
                          configs[config_index];

        for (int i = 0; i < 8; i++) {
          int event = (config >> (8 * i)) & 0xFF;
          if (!event)
            continue;

          uint64_t value = g_counters1[i + 2];
          if (counts[event] >= OUTER_ITS) {
            printf("error\n");
            exit(1);
          }
          results[event][counts[event]++] = value;
          sum_diffs[event] += value;
        }
      }
    }
  }

  int first = 1;
  printf("{\n");
  for (int event = 1; event < 0x100; event++) {
    if (sum_diffs[event] == 0)
      continue;
    if (counts[event] != OUTER_ITS) {
      printf("error at event %d\n", event);
      exit(1);
    }
    if (first) {
      first = 0;
    } else {
      printf(",\n");
    }
    printf(" \"%02x\": [", event);

    for (size_t i = 0; i < counts[event]; i++) {
      if (i != 0)
        printf(", ");
      printf("%llu", results[event][i]);
    }
    printf("]");
  }
  printf("\n}\n");

  return 0;
}
