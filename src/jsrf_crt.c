#include "recomp/gen/recomp_types.h"
#include <stdio.h>

/* 0x0017CEC0: CRT memmove, confirmed by the XBE's overlap check and
 * forward/backward copy paths. The generated body truncates its backward
 * jump table at 0x0017D15A and dispatches internal labels as functions.
 * cdecl: [esp+4]=dst, [esp+8]=src, [esp+12]=size; return dst in eax.
 * Only RET's four bytes belong to the callee. Preserve nonvolatile registers.
 */
void sub_0017CEC0(void)
{
    uint32_t destination = MEM32(g_esp + 4);
    uint32_t source = MEM32(g_esp + 8);
    uint32_t size = MEM32(g_esp + 12);
    static RECOMP_TLS int reported;

    if (size != 0)
        memmove((void *)XBOX_PTR(destination), (const void *)XBOX_PTR(source), size);
    g_eax = destination;
    g_esp += 4;
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[JSRF] memmove 0x0017CEC0: copied %u bytes "
                "0x%08X -> 0x%08X; returned with esp=0x%08X\n",
                size, source, destination, g_esp);
    }
}

/* The CRT's 64-bit divide helpers.
 *
 * Their generated bodies drop every `rcr` (the lifter of the translation pass
 * that produced src/recomp/gen did not lift rcl/rcr; upstream fixed that in
 * 4dd267a), so the normalisation loop that runs whenever the divisor needs
 * more than 32 bits shifted only the high halves and returned wrong results.
 * These replace the generated bodies; see docs/reviews/crt-64bit-divide.md.
 *
 * All four are stdcall with four dword arguments -- dividend low/high, then
 * divisor low/high -- and `ret 0x10`, so the callee pops the return address
 * plus 16 bytes. Registers follow the XBE's own bodies (disassembly checked):
 *
 *   0x0017C9C0 __alldiv    signed quotient    -> edx:eax; keeps ebx esi edi
 *   0x0017D4D0 __aulldiv   unsigned quotient  -> edx:eax; keeps ebx esi
 *   0x0017D2C0 __aullrem   unsigned remainder -> edx:eax; keeps ebx
 *   0x001816B0 __aulldvrm  unsigned quotient  -> edx:eax,
 *                          unsigned remainder -> ebx:ecx; keeps esi
 *
 * The guest divides, so a zero divisor raises the divide-error exception; the
 * host division below raises the same one. ecx is volatile in every calling
 * convention the title uses and is left unspecified except where __aulldvrm
 * defines it. */
static uint64_t crt_arg64(uint32_t offset)
{
    return (uint64_t)MEM32(g_esp + offset) | ((uint64_t)MEM32(g_esp + offset + 4) << 32);
}

static void crt_return64(uint64_t value)
{
    g_eax = (uint32_t)value;
    g_edx = (uint32_t)(value >> 32);
    g_esp += 4 + 16;
}

/* 0x0017C9C0: __alldiv. Divides magnitudes and negates when exactly one
 * operand is negative, as the XBE does, so INT64_MIN / -1 wraps to INT64_MIN
 * instead of being undefined. */
void sub_0017C9C0(void)
{
    uint64_t dividend = crt_arg64(4), divisor = crt_arg64(12);
    int negative = 0;
    if ((int64_t)dividend < 0) { dividend = 0 - dividend; negative ^= 1; }
    if ((int64_t)divisor < 0) { divisor = 0 - divisor; negative ^= 1; }
    uint64_t quotient = dividend / divisor;
    crt_return64(negative ? 0 - quotient : quotient);
}

/* 0x0017D4D0: __aulldiv. */
void sub_0017D4D0(void)
{
    crt_return64(crt_arg64(4) / crt_arg64(12));
}

/* 0x0017D2C0: __aullrem. */
void sub_0017D2C0(void)
{
    crt_return64(crt_arg64(4) % crt_arg64(12));
}

/* 0x001816B0: __aulldvrm. */
void sub_001816B0(void)
{
    uint64_t dividend = crt_arg64(4), divisor = crt_arg64(12);
    uint64_t quotient = dividend / divisor, remainder = dividend % divisor;
    g_ecx = (uint32_t)remainder;
    g_ebx = (uint32_t)(remainder >> 32);
    crt_return64(quotient);
}
