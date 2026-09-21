#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "recomp_types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The kernel bridge references the game dispatcher/diagnostics; this focused
 * ABI fixture does not start guest threads, so keep those hooks inert. */
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site,
                        uint32_t value)
{ (void)kind; (void)target; (void)site; (void)value; }

int main(void)
{
    uint8_t *vram = (uint8_t *)calloc(1, 64u * 1024u * 1024u);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint32_t value = 0;
    uint8_t byte = 0;
    uint8_t invalid[8];
    int ok = vram && ramin;
    if (!ok || !nv2a_init_standalone(vram, 64u * 1024u * 1024u,
                                     ramin, 1024u * 1024u)) return 2;

    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, FALSE);
    if (value != 0x02A010DEu) fprintf(stderr, "identity read %08X\n", value), ok = 0;
    value = 0xFFFFFFFFu;
    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, FALSE);
    if (value != 0x02A010DEu) fprintf(stderr, "identity write changed %08X\n", value), ok = 0;
    value = 0xFFFFFFFFu;
    xbox_HalReadWritePCISpace(1, 0, 0x08, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x08, &value, 4, FALSE);
    if (value != 0x030000A1u) fprintf(stderr, "class write changed %08X\n", value), ok = 0;

    value = 0x11223344u;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, FALSE);
    if (value != 0x11223344u) fprintf(stderr, "vendor read %08X\n", value), ok = 0;
    byte = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4F, &byte, 1, FALSE);
    byte |= 0x1Fu;
    xbox_HalReadWritePCISpace(1, 0, 0x4F, &byte, 1, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, FALSE);
    if (value != 0x1F223344u) fprintf(stderr, "vendor byte read %08X\n", value), ok = 0;

    value = 0xA5A5A5A5u;
    xbox_HalReadWritePCISpace(0, 0, 0x4C, &value, 4, FALSE);
    if (value != 0) fprintf(stderr, "other bus read %08X\n", value), ok = 0;
    value = 0xA5A5A5A5u;
    xbox_HalReadWritePCISpace(1, 0, 0x7F, &value, 2, FALSE);
    if (value != 0xA5A50000u) fprintf(stderr, "crossing read %08X\n", value), ok = 0;
    memset(invalid, 0xA5, sizeof(invalid));
    xbox_HalReadWritePCISpace(1, 0, 0x4C, invalid, 8, FALSE);
    for (size_t i = 0; i < sizeof(invalid); ++i)
        if (invalid[i] != 0x00) ok = 0;

    free(ramin);
    free(vram);
    if (!ok) {
        fprintf(stderr, "FAIL: HAL PCI bridge contract\n");
        return 1;
    }
    puts("PASS: HAL PCI bridge ABI/config contract");
    return 0;
}
