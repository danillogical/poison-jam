/*
 * Test-only shared oracles for the Vulkan sub-allocator counters.
 *
 * The real workload gate (`tests/test_gpu_backend_parity_vk.c`, run_suballoc) and the
 * controlled production-allocator gate (`tests/test_vk_suballoc_gate.c`, property 11) must
 * decide the SAME predicates.  These bodies are that single definition: the workload calls
 * them on its own counters and the controlled gate calls them on the counters of its
 * deliberately padded / dedicated scenario.  A private mutation here therefore has to change
 * both consumers; a copied expression in either fixture cannot stand in for this binding.
 *
 * Test-only: no production file includes this header, and nothing here touches the backend.
 */
#ifndef JSRF_VK_SUBALLOC_ORACLES_H
#define JSRF_VK_SUBALLOC_ORACLES_H

#include <stdint.h>

/* `nv2a_vk_backend_memory_stats` publishes reserved = used + free + padding.  The subtraction
 * form cannot overflow and rejects any field that exceeds the reservation. */
static int vk_suballoc_bytes_close(uint64_t reserved, uint64_t used,
                                   uint64_t free_bytes, uint64_t padding)
{
    if (used > reserved || free_bytes > reserved || padding > reserved)
        return 0;
    return reserved - used - free_bytes == padding;
}

/* The workload's reduction claim: pooled ranges (alloc_calls minus the dedicated allocations
 * already included in alloc_calls) cost at most half as many block allocations as ranges.
 * `alloc_calls < dedicated` is a broken counter identity, not a pass. */
static int vk_suballoc_reduction_close(uint64_t alloc_calls, uint64_t dedicated,
                                       uint64_t ranges)
{
    if (alloc_calls < dedicated)
        return 0;
    return alloc_calls - dedicated <= ranges / 2u;
}

/* Every dedicated allocation increments exactly one reason counter: required, preferred,
 * oversize, no pool slot, non-coherent host-visible, or a block-creation fallback.  The
 * success-path identity is what a failure path is not allowed to excuse. */
static int vk_suballoc_dedicated_reasons_close(uint64_t dedicated, uint64_t required,
                                               uint64_t prefer, uint64_t oversize,
                                               uint64_t noroom, uint64_t nohost,
                                               uint64_t fallbacks)
{
    uint64_t sum = required + prefer + oversize + noroom + nohost + fallbacks;

    if (sum < required)                   /* overflow in the reason sum */
        return 0;
    return dedicated == sum;
}

#endif /* JSRF_VK_SUBALLOC_ORACLES_H */
