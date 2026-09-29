/* The CRT 64-bit divide helpers in src/jsrf_crt.c, against native arithmetic.
 *
 * Each case lays out the guest stack the way the title calls them -- return
 * address, dividend low/high, divisor low/high -- and checks the result
 * registers, the stdcall `ret 0x10` stack adjustment, and that the registers
 * each XBE body preserves are unchanged. Divisors above 32 bits are the cases
 * the generated bodies got wrong, so they are the bulk of the table. */
#include <stdio.h>
#include "recomp/gen/recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_esp, g_ecx, g_edx, g_ebx, g_esi, g_edi;
extern void sub_0017C9C0(void); /* __alldiv */
extern void sub_0017D4D0(void); /* __aulldiv */
extern void sub_0017D2C0(void); /* __aullrem */
extern void sub_001816B0(void); /* __aulldvrm */

static unsigned char guest[256];
static unsigned failures;

static void setup(uint64_t a, uint64_t b)
{
    g_esp = 64;
    MEM32(64) = 0x11223344;                 /* return address */
    MEM32(68) = (uint32_t)a; MEM32(72) = (uint32_t)(a >> 32);
    MEM32(76) = (uint32_t)b; MEM32(80) = (uint32_t)(b >> 32);
    g_eax = g_edx = g_ecx = 0xDEADBEEF;
    g_ebx = 0xB0B0B0B0; g_esi = 0x51515151; g_edi = 0xD1D1D1D1;
}

static void expect(const char *name, uint64_t a, uint64_t b, uint64_t want,
                   int keeps_ebx)
{
    uint64_t got = ((uint64_t)g_edx << 32) | g_eax;
    int ok = got == want && g_esp == 64 + 20 && g_esi == 0x51515151 &&
             g_edi == 0xD1D1D1D1 && (!keeps_ebx || g_ebx == 0xB0B0B0B0);
    if (!ok) {
        failures++;
        fprintf(stderr, "FAIL %s(%llx, %llx): got %llx want %llx esp=%u\n",
                name, (unsigned long long)a, (unsigned long long)b,
                (unsigned long long)got, (unsigned long long)want, g_esp);
    }
}

int main(void)
{
    static const uint64_t values[] = {
        1, 2, 3, 7, 10, 1000, 0xFFFFFFFFull, 0x100000000ull, 0x100000001ull,
        0x1FFFFFFFFull, 0x123456789ull, 0x2540BE400ull /* 10^10 */,
        0x8000000000000000ull, 0x7FFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull,
        0xFFFFFFFF00000000ull, 0x00000001FFFFFFFFull, 0xDEADBEEFCAFEF00Dull,
        0x0000123456789ABCull, 0x8000000000000001ull,
        (uint64_t)-1000, (uint64_t)-0x100000000ll, (uint64_t)-7,
    };
    const unsigned n = sizeof(values) / sizeof(values[0]);
    unsigned cases = 0;
    g_xbox_mem_offset = (ptrdiff_t)guest;

    for (unsigned i = 0; i < n; ++i) {
        for (unsigned j = 0; j < n; ++j) {
            uint64_t a = values[i], b = values[j];
            int64_t sa = (int64_t)a, sb = (int64_t)b;
            uint64_t want_s;

            /* __alldiv: truncating signed division; INT64_MIN / -1 wraps. */
            if (sa == INT64_MIN && sb == -1)
                want_s = (uint64_t)INT64_MIN;
            else
                want_s = (uint64_t)(sa / sb);
            setup(a, b); sub_0017C9C0(); expect("alldiv", a, b, want_s, 1);

            setup(a, b); sub_0017D4D0(); expect("aulldiv", a, b, a / b, 1);
            setup(a, b); sub_0017D2C0(); expect("aullrem", a, b, a % b, 1);

            setup(a, b); sub_001816B0();
            expect("aulldvrm", a, b, a / b, 0);
            if ((((uint64_t)g_ebx << 32) | g_ecx) != a % b) {
                failures++;
                fprintf(stderr, "FAIL aulldvrm remainder(%llx, %llx)\n",
                        (unsigned long long)a, (unsigned long long)b);
            }
            cases += 4;
        }
    }
    if (failures) {
        fprintf(stderr, "%u failure(s) in %u cases\n", failures, cases);
        return 1;
    }
    printf("crt 64-bit divide: %u cases OK\n", cases);
    return 0;
}
