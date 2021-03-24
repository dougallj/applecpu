#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <sys/sysctl.h>

#define SREG_PMCR0 "S3_1_c15_c0_0"
#define SREG_PMCR1 "S3_1_c15_c1_0"
#define SREG_PMCR2 "S3_1_c15_c2_0"
#define SREG_PMCR3 "S3_1_c15_c3_0"
#define SREG_PMCR4 "S3_1_c15_c4_0"
#define SREG_PMESR0 "S3_1_c15_c5_0"
#define SREG_PMESR1 "S3_1_c15_c6_0"
#define SREG_PMSR "S3_1_c15_c13_0"
#define SREG_OPMAT0 "S3_1_c15_c7_0"
#define SREG_OPMAT1 "S3_1_c15_c8_0"
#define SREG_OPMSK0 "S3_1_c15_c9_0"
#define SREG_OPMSK1 "S3_1_c15_c10_0"

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

#define SREG_PMMMAP "S3_2_c15_c15_0"
#define SREG_PMTRHLD2 "S3_2_c15_c14_0"
#define SREG_PMTRHLD4 "S3_2_c15_c13_0"
#define SREG_PMTRHLD6 "S3_2_c15_c12_0"

#define SREG_WRITE(SR, V) __asm__ volatile("msr " SR ", %0 ; isb" : : "r"(V))
#define SREG_READ(SR)                                                          \
  ({                                                                           \
    uint64_t VAL;                                                              \
    __asm__ volatile("mrs %0, " SR : "=r"(VAL));                               \
    VAL;                                                                       \
  })

static int sysctl_pmesr0 SYSCTL_HANDLER_ARGS {
  uint64_t in = -1;
  int error = SYSCTL_IN(req, &in, sizeof(in));

  if (!error && req->newptr) {
    printf("PMCKext2: pmesr1 = %llx\n", in);
    SREG_WRITE(SREG_PMESR0, in);
  } else if (!error) {
    printf("PMCKext2: read pmesr1\n");
    uint64_t out = SREG_READ(SREG_PMESR0);
    error = SYSCTL_OUT(req, &out, sizeof(out));
  }

  if (error) {
    printf("PMCKext2: sysctl_pmesr0 failed with error %d\n", error);
  }

  return error;
}

SYSCTL_PROC(_kern, OID_AUTO, pmesr0,
            CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_ANYBODY, NULL, 0,
            &sysctl_pmesr0, "I", "pmesr0");

static int sysctl_pmesr1 SYSCTL_HANDLER_ARGS {
  uint64_t in = -1;
  int error = SYSCTL_IN(req, &in, sizeof(in));

  if (!error && req->newptr) {
    printf("PMCKext2: pmesr1 = %llx\n", in);
    SREG_WRITE(SREG_PMESR1, in);
  } else if (!error) {
    printf("PMCKext2: read pmesr1\n");
    uint64_t out = SREG_READ(SREG_PMESR1);
    error = SYSCTL_OUT(req, &out, sizeof(out));
  }

  if (error) {
    printf("PMCKext2: sysctl_pmesr1 failed with error %d\n", error);
  }

  return error;
}

SYSCTL_PROC(_kern, OID_AUTO, pmesr1,
            CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_ANYBODY, NULL, 0,
            &sysctl_pmesr1, "I", "pmesr1");

static int sysctl_pmcr0 SYSCTL_HANDLER_ARGS {
  uint64_t in = -1;
  int error = SYSCTL_IN(req, &in, sizeof(in));

  if (!error && req->newptr) {
    printf("PMCKext2: pmcr0 = %llx\n", in);
    SREG_WRITE(SREG_PMCR0, in);
  } else if (!error) {
    printf("PMCKext2: read pmcr0\n");
    uint64_t out = SREG_READ(SREG_PMCR0);
    error = SYSCTL_OUT(req, &out, sizeof(out));
  }

  if (error) {
    printf("PMCKext2: sysctl_pmcr0 failed with error %d\n", error);
  }

  return error;
}

SYSCTL_PROC(_kern, OID_AUTO, pmcr0, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_ANYBODY,
            NULL, 0, &sysctl_pmcr0, "I", "pmcr0");

static int sysctl_pmcr1 SYSCTL_HANDLER_ARGS {
  uint64_t in = -1;
  int error = SYSCTL_IN(req, &in, sizeof(in));

  if (!error && req->newptr) {
    printf("PMCKext2: pmcr1 = %llx\n", in);
    SREG_WRITE(SREG_PMCR1, in);
  } else if (!error) {
    printf("PMCKext2: read pmcr1\n");
    uint64_t out = SREG_READ(SREG_PMCR1);
    error = SYSCTL_OUT(req, &out, sizeof(out));
  }

  if (error) {
    printf("PMCKext2: sysctl_pmcr1 failed with error %d\n", error);
  }

  return error;
}

SYSCTL_PROC(_kern, OID_AUTO, pmcr1, CTLTYPE_QUAD | CTLFLAG_RW | CTLFLAG_ANYBODY,
            NULL, 0, &sysctl_pmcr1, "I", "pmcr1");

kern_return_t PMCKext2_start(kmod_info_t *ki, void *d);
kern_return_t PMCKext2_stop(kmod_info_t *ki, void *d);

kern_return_t PMCKext2_start(kmod_info_t *ki, void *d) {
  printf("PMCKext2_start\n");
  sysctl_register_oid(&sysctl__kern_pmesr0);
  sysctl_register_oid(&sysctl__kern_pmesr1);
  sysctl_register_oid(&sysctl__kern_pmcr0);
  sysctl_register_oid(&sysctl__kern_pmcr1);
  return KERN_SUCCESS;
}

kern_return_t PMCKext2_stop(kmod_info_t *ki, void *d) {
  printf("PMCKext2_stop\n");
  sysctl_unregister_oid(&sysctl__kern_pmesr0);
  sysctl_unregister_oid(&sysctl__kern_pmesr1);
  sysctl_unregister_oid(&sysctl__kern_pmcr0);
  sysctl_unregister_oid(&sysctl__kern_pmcr1);
  return KERN_SUCCESS;
}
