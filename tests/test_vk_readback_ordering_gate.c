/* N4.2's readback publication, as a deciding ORDERING regression: a batch that could not stage
 * all of its targets leaves its already-recorded copies in the open recording, and the NEXT
 * group's slices start at offset 0 again -- so the two groups' copies write the same staging
 * bytes. Nothing orders those two device writes against each other unless the abandonment
 * records an ordering command before it steps over the group.
 *
 * The defect this case exists for
 * -------------------------------
 * vb_targets_flush publishes dirty targets in bounded groups: one group is one submission
 * carrying every slice that fits VB_READBACK_MAX (a 4-byte target copied straight out of its
 * image), and one fence wait and one memcpy per row into guest memory per group. When a group's
 * pack descriptor set cannot be allocated the code sets `batch_ok` to 0 and does
 *
 *     i = j;
 *     continue;
 *
 * -- the group submits nothing and publishes nothing, and every staged target of it keeps its
 * dirty flag and is retried. But the copies the group had ALREADY recorded are not undone: they
 * stay in the group's recording command buffer and are submitted with the next submission the
 * back end makes, which is the next group's. That next group's slice offsets start at 0 again,
 * because the slice offsets are computed from the group's own first target -- so its
 * vkCmdCopyImageToBuffer writes the same g.rb_buf bytes the abandoned recording's LAST write
 * wrote. That last write is the vulnerable one, and it is one of two classes: a 4-byte target's
 * own image copy, or a 16-bit target's pack-output buffer copy (g.pack_buf -> g.rb_buf). Both
 * land in g.rb_buf at the same offset as the next group's first copy, so Vulkan requires a memory
 * dependency between the two writes
 * (https://docs.vulkan.org/spec/latest/chapters/synchronization.html), and nothing between them
 * provides one: the batch's own barrier -- `srcAccessMask = SHADER_WRITE|TRANSFER_WRITE`,
 * `dstAccessMask = HOST_READ`, into VK_PIPELINE_STAGE_HOST_BIT -- is recorded AFTER both writes,
 * and its destination access is a host READ, which does not order two device writes.
 *
 * The pack KERNEL's write into g.pack_buf is ordered differently from the staging copies, and this
 * case says so rather than treating every staging write alike: the pack path's own barrier -- the
 * one between its dispatch and the buffer copy that reads that slice -- now orders ONLY that
 * dispatch's write before that copy's read (`COMPUTE_SHADER/SHADER_WRITE ->
 * TRANSFER/TRANSFER_READ`), so it does NOT cover the pack buffer's reuse across an abandoned group
 * boundary -- a later dispatch writing the g.pack_buf bytes an abandoned group's copy read. The
 * broad abandonment barrier the repair records covers that class, and this case decides it on that
 * barrier (1c and 1d). What the defect breaks is the g.rb_buf class: the last staging write the
 * abandoned recording left behind. The device may execute it after the new group's own copy, and
 * the host's memcpy then publishes bytes the abandoned group wrote over the new group's image, so
 * guest memory silently holds the wrong rectangle. This is a write-after-write hazard inside one
 * command buffer, and it is a correctness bug independent of what the host reads afterwards.
 *
 * The repair is exactly one ordering command at the abandonment point: `vb_barrier();`, the
 * file's own broad ALL_COMMANDS->ALL_COMMANDS, MEMORY_WRITE->(MEMORY_READ|MEMORY_WRITE) barrier,
 * before `i = j; continue;`. This case is the ordering decision that measures it -- NOT a
 * narrower or targeted barrier, and not the host-visibility barrier the batch already records.
 *
 * The production bodies are the toolkit's own
 * ------------------------------------------
 * This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND
 * defined, so every function driven here -- vb_targets_flush, vb_targets_flush_recover,
 * vb_readback_recover, vb_flip, vb_readback_slice, vb_readback_reserve, vb_transfer_cmd, vb_cmd,
 * vb_reserve, vb_slot_submit, vb_slot_wait, vb_drain, vb_barrier -- is compiled from the
 * production source, verbatim. Nothing is copied, so nothing can drift: the build recompiles
 * these bodies whenever that file changes, and scripts/vk-readback-ordering-gate.py probes the
 * same bytes at build time and FAILS THE BUILD (exit 2) if a definition, a struct field or a
 * global the fixture binds to is gone. That script also derives the seam markers in
 * vk_readback_ordering_gate_seam.h from those bytes; the PRE-FIX source must still build -- it is
 * what the case reports against -- so the repair's own seam is a printed marker (and a recorded
 * stream the case decides), never a required anchor.
 *
 * Nothing of the toolkit is linked: the file's five undefined names are defined at the bottom of
 * this TU (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack), and every Vulkan entry point it uses is a static
 * function pointer (VK_NO_PROTOTYPES) that this TU points at controlled stubs before the first
 * call. The stubs RECORD the command stream: every write (vkCmdCopyImageToBuffer into g.rb_buf,
 * vkCmdCopyBuffer g.pack_buf->g.rb_buf, and the pack dispatch's shader write into g.pack_buf,
 * reconstructed from the push block), every vkCmdPipelineBarrier with its stage flags and access
 * masks, and every submit, fence wait, fence reset and command-buffer reset, each with a sequence
 * number. That recording is the decision.
 *
 * The injected result is a STUBBED DRIVER OUTCOME
 * -----------------------------------------------
 * There is no GPU here and no ICD is needed: vkAllocateDescriptorSets returns the VkResult this
 * fixture selects (exactly once, or on every call), vkWaitForFences returns VK_SUCCESS, and the
 * recording entry points only record. VK_ERROR_OUT_OF_POOL_MEMORY below is a stub's return value,
 * NOT a real pool exhaustion; the "[GPUVK] vkAllocateDescriptorSets failed" line on stderr is the
 * production code reporting the injected outcome. The fixture also stands in for the GPU's own
 * writes: the staging buffer vkMapMemory hands back is filled by the test with the bytes the copy
 * engine (4-byte target) or the pack kernel (16-bit target) would have produced, so what guest
 * memory must hold afterwards is exact, byte for byte.
 *
 * The fixture's two scenarios, three targets each
 * ----------------------------------------------
 * Production-legal dimensions, disjoint guest ranges contiguous from 0, dirty flags set, wired
 * directly as VbTargets. Scenario A, whose abandoned group's last write is a 4-byte image copy:
 *   T0  4-byte 4096x4096, pitch 16384, dirty rectangle 4096x4096 -> slice 64 MiB (67108864)
 *   T1  16-bit 4096x4096, pitch  8192, dirty rectangle 4096x4096 -> packed slice 32 MiB
 *   T2  4-byte 64x64,     pitch   256, dirty rectangle 64x64     -> slice 16384
 * with the pack-buffer variant of T2: 16-bit 64x64, pitch 128 -> packed slice 8192, whose own
 * pack dispatch and buffer copy are the recording's later writes.
 * T0 + T1 fill VB_READBACK_MAX (96 MiB) exactly, so T2 starts the NEXT group with slice offset 0
 * -- which is precisely what makes its copy write the bytes the abandoned copy wrote.
 * Scenario B, whose abandoned group's last write is the OTHER class, a 16-bit target's pack-output
 * buffer copy:
 *   TC  16-bit 4096x4096, pitch 8192, dirty rectangle 4096x4096 -> packed slice 32 MiB, and it is
 *       the group's FIRST staged target, so its pack set allocates (allocation 1) and its slice is
 *       at offset 0: the group records its dispatch, its pack barrier and its vkCmdCopyBuffer into
 *       g.rb_buf [0, 32 MiB) before anything is abandoned;
 *   TD  16-bit 128x64,   pitch  256, dirty rectangle 128x64     -> packed slice 16 KiB, and its
 *       pack set IS the injected failure (allocation 2), so the group is abandoned after TC's
 *       buffer copy -- which is then the abandoned recording's last write;
 *   TE  4-byte 4096x4096, pitch 16384, dirty rectangle 4096x4096 -> slice 64 MiB; TC + TD do not
 *       fill the cap, so TE does not fit and starts the next group at slice offset 0, and its own
 *       vkCmdCopyImageToBuffer writes g.rb_buf [0, 64 MiB) in the SAME command buffer as TC's
 *       abandoned buffer copy.
 * Scenario C, whose abandoned recording's LAST commands are a successful 16-bit target's pack
 * path, so the class the writes alone cannot show appears:
 *   TC2 16-bit 4096x4096, pitch 8192, dirty rectangle 4096x4096 -> packed slice 32 MiB, and it is
 *       the group's FIRST staged target, so its pack set allocates (allocation 1), its slice is at
 *       offset 0, and the buffer copy it records READS g.pack_buf [0, 32 MiB);
 *   TX  4-byte 4096x2048, pitch 16384, dirty rectangle 4096x2048 -> slice 32 MiB, the group's
 *       second slice, at offset 33554432;
 *   TD2 16-bit 128x64,   pitch  256, dirty rectangle 128x64     -> packed slice 16 KiB, and its
 *       pack set IS the injected failure (allocation 2), so the group is abandoned after TC2's
 *       pack path;
 *   TF  16-bit 4096x4096, pitch 8192, dirty rectangle 4096x4096 -> packed slice 32 MiB; 64 MiB +
 *       16 KiB does not leave room for it, so it starts the next group at slice offset 0 and its
 *       OWN dispatch WRITES g.pack_buf [0, 32 MiB) -- the write-after-READ pair with the abandoned
 *       read, which a log of writes alone cannot see at all -- while its own buffer copy writes
 *       the g.rb_buf bytes TC2's abandoned copy wrote (the write-after-write class, again).
 * The scenarios' footprints end exactly where each other's do, so one guest arena and one watched
 * tail serve all three.
 * Each rectangle is the target's whole guest surface (dx0 = dy0 = 0, w*bpp == pitch, full height),
 * so a published rectangle is exactly its slice's bytes and neither side has slack inside it.
 *
 * The injection
 * -------------
 * vkAllocateDescriptorSets returns VK_ERROR_OUT_OF_POOL_MEMORY exactly once, at a chosen call: in
 * scenario A at T1's pack set -- AFTER T0's vkCmdCopyImageToBuffer was recorded and while T1's own
 * dispatch has not been -- and in scenario B at TD's, after TC's whole pack path. The
 * permanent-failure forms make every allocation fail, or every allocation from the second on (so
 * the abandoned group still records a pack path). These are the same stub seams the readback fault
 * gate uses, configured by a call index.
 *
 * The fixture supplies the device's bytes at each batch's fence wait
 * -----------------------------------------------------------------
 * The staging buffer is filled by the fixture with the bytes the copy engine or the pack kernel
 * would have produced, and it does so when a batch's fence wait is behind it -- at the
 * invalidation the publication performs immediately before it memcpys -- rather than up front.
 * That matters because two batches' slices share bytes (both start at their own group's slice
 * offset 0): supplying per batch is what makes every published rectangle exactly that target's
 * own bytes. Which order the device picked for an unordered pair is NOT modelled (the writes are
 * supplied in recording order, the benign one) and nothing here decides on the bytes: the case
 * decides the recorded stream.
 *
 * The ordering decision
 * ---------------------
 * Two arrows are decided, and they need different things from a barrier:
 *
 * write-after-write -- for every pair of OVERLAPPING WRITES to the same buffer, in recording
 * order:
 *   - same recording: the earlier write is ordered before the later one only if a barrier
 *     recorded STRICTLY between them has srcStage covering the earlier writer's stage,
 *     srcAccessMask covering its access (TRANSFER_WRITE for a copy, SHADER_WRITE for the pack
 *     dispatch), dstStage covering the later writer's stage and dstAccessMask COVERING the later
 *     writer's access -- a memory dependency is what this arrow needs, so a destination access
 *     that does not name the later write (a host READ, say) does not order two device writes, no
 *     matter what it names for the earlier one, and a barrier recorded after both does not count.
 *     A barrier's VK_ACCESS_MEMORY_WRITE_BIT/READ_BIT source or destination access is the spec's
 *     "every write"/"every read" bit and covers any access of that kind;
 *     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT covers either stage.
 *   - different recordings: ordered only if the earlier one was ended, submitted, and its fence
 *     wait completed before the later command was recorded (a submission/fence boundary; the ring
 *     reuses one command buffer handle for later recordings, so this is per RECORDING, not per
 *     handle).
 *
 * write-after-read (WAR) -- for every pair of an earlier READ and a later WRITE of overlapping
 * bytes of the same buffer. What this arrow needs is an EXECUTION dependency, and the case is
 * strict about not asking for more: a barrier strictly between them whose srcStage covers the
 * READER's stage and whose dstStage covers the LATER WRITER's stage is enough, and NO access mask
 * is consulted at all -- in particular the destination access is not required to name anything,
 * because a read does not become "available" the way a write does. The device read the readback
 * path performs is the pack path's buffer copy reading the packed slice out of g.pack_buf, and
 * the class is the device-pack-buffer dependency this case exists to cover: a later group's pack
 * dispatch writes those same pack buffer bytes at its own group's slice offset 0.
 *
 * Cross-recording pairs of either arrow use the same submission-and-fence boundary, and that rule
 * is deliberately STRICTER than Vulkan: a submission whose fence wait completed certainly orders
 * what follows it, but Vulkan also accepts other valid arrangements (a queue-family ownership
 * transfer, two submissions with their own signal/wait pairs, a semaphore wait) that this gate
 * does not model. It decides THIS implementation's command stream and fails safe rather than
 * accepting every legal alternative.
 *
 * The case reports the `overlapping reuses` and `unordered` counts for the write-after-write
 * arrow and the `WAR pairs` and `war_unordered` counts for the write-after-read arrow, and quotes,
 * for every pair, both ranges, the buffer handles, the command buffers, the stages -- and the
 * barrier or the submission/fence boundary that orders it, or the word UNORDERED.
 *
 * The properties
 * --------------
 *   0. the source and its seam        -- the file compiled here is the one the case was built
 *      against (its sha256 and the five seam markers are printed), the fixture's geometry is the
 *      source's own (its slices are vb_readback_slice's answers), and the three slices and two
 *      groups fit the source's bounds.
 *   1a. the failing pass, 4-byte third target -- T0's copy (dest g.rb_buf [0, 67108864)) and T2's
 *      copy (dest [0, 16384)) are recorded into the SAME command buffer with no submit or wait
 *      between them; the reuse is real, exactly one barrier lies between them, and that barrier is
 *      the broad abandonment barrier with the masks and stages quoted. RED on the pre-fix source:
 *      no barrier is between them at all, so the reuse is unordered.
 *   1b. the failing pass, 16-bit third target (the pack-buffer variant of scenario A) --
 *      the third target's pack dispatch writes g.pack_buf [0, 8192) and its vkCmdCopyBuffer
 *      overwrites g.rb_buf [0, 8192) after the abandoned T0 copy. The reuse is real and ordered,
 *      and the barrier that orders it is the broad abandonment barrier -- the pack path's own
 *      barrier no longer covers it, because it names the dispatch's write and the copy's read of
 *      that slice and no destination write. This variant therefore decides the repair too. RED on
 *      the pre-fix source and on both mutants.
 *   1c. the failing pass, scenario B     -- the abandoned recording's last write is TC's
 *      pack-OUTPUT buffer copy into g.rb_buf [0, 33554432), and TE's image copy writes the same
 *      buffer's bytes at [0, 67108864) in the SAME command buffer with no submit or wait between
 *      them. The case asserts: both writes were recorded and the failure came after them; that
 *      copy is the recording's last write before the abandonment; the abandoned dispatch's
 *      g.pack_buf write is separated from the next group's staging write by the broad abandonment
 *      barrier (the pack path's own barrier orders only that dispatch's write before the copy that
 *      reads its slice, so the pack-buffer class across a group boundary is the abandonment
 *      barrier's job); the abandoned group submitted and waited for nothing of its own, its
 *      recording still open in slot 0; the reuse is real, exactly one barrier lies between the two
 *      g.rb_buf writes, and it is the broad abandonment barrier with the masks and stages quoted;
 *      TE was published exactly once from its own bytes while TC and TD stayed dirty; and the
 *      failure is not a device loss. RED on the pre-fix source and on both mutants.
 *   1d. the failing pass, scenario C     -- the abandoned recording read g.pack_buf [0, 33554432)
 *      through TC2's buffer copy, and the NEXT group's own dispatch writes those same bytes, in
 *      the same command buffer with no barrier between them pre-fix: the write-after-READ pair.
 *      The case asserts the read's own range and stage, that the abandoned group's pack path was
 *      recorded in full, that the next group's dispatch writes exactly those bytes, that the pair
 *      is decided (WAR pairs 1, war_unordered 0) by the abandonment barrier covering both stages
 *      with NO access mask consulted, that the two dispatches' own reuse of the pack buffer is
 *      ordered by the broad abandonment barrier -- the pack path's own barrier covers neither of
 *      those pairs, since it names one dispatch's write and the read of its own slice -- that the
 *      abandoned batch submitted nothing of its own, that TF published exactly its own bytes while
 *      TX, TC2 and TD2 stayed dirty, and that the failure is not a device loss. RED on the pre-fix
 *      source and on both mutants.
 *   2. the failing pass, as a state  -- T0 and T1 still dirty; the abandoned batch submitted and
 *      waited for nothing of its own (its copies stayed in the recording slot 0 had left open, and
 *      the pass's next submission carried them; that pass's CALL does submit and wait once, for
 *      the third target's own group, which is exactly how the abandoned copies reach the device
 *      at all); the third target was published exactly once from the bytes the fixture supplied
 *      for THAT target's own slice; readback_incomplete and g.failed still 0 and g.ok still 1 --
 *      a transient pool failure is not a device loss.
 *   3. the recovery control through the boundary -- vb_targets_flush_recover (the entry point the
 *      flips and the release paths drive) publishes all three rectangles exactly, every published
 *      byte compared with the bytes the fixture supplied for that target when the batch that
 *      published it passed its fence, with exactly one retry, readback_incomplete 0, g.ok 1,
 *      g.failed 0, the slot free again (pending 0, npending 0, fence and pool reset) before the
 *      retry recorded, and NO unordered reuse anywhere across both passes.
 *   3b. the recovery control, scenario B -- the same boundary on scenario B: all three rectangles
 *      published exactly (TC and TD on the retry, from their own supplied bytes), exactly one
 *      retry, readback_incomplete 0, g.ok 1, g.failed 0, the slot free before the retry recorded,
 *      and NO unordered reuse across both passes -- where the deciding reuse is the abandoned
 *      pack-output buffer copy against TE's copy. RED on the pre-fix source and on both mutants.
 *   4. the permanent-failure control     -- driven through the real vb_flip boundary with EVERY
 *      pack-set allocation failing: the boundary stops (g.ok 0, g.failed 1, the two unpublished
 *      targets counted as readback_incomplete), T0's and T1's rectangles are byte-identical to
 *      their pre-call guest bytes, the third target still holds its own correct rectangle, no
 *      frame advances and no target is marked stale, and there is NO unordered reuse anywhere.
 *      This is the property the defect breaks: pre-fix, the abandoned copy and the third target's
 *      copy are the same buffer's bytes with nothing between them.
 *   4b. the permanent-failure form, scenario B with every allocation failing -- the same stop and
 *      the same counting, and TE still publishes its own rectangle. This form's first failure is
 *      TC's own set, so the abandoned group records NO write at all and no reuse exists to be
 *      unordered: the case states that (overlapping reuses 0) instead of dressing a "0 of 0" up
 *      as an ordered reuse. Green on every source, and reported as a control.
 *   4c. the permanent-failure form, scenario B with the failure persisting at TD's set -- the form
 *      that DOES record the abandoned pack path: every attempt abandons the group after TC's
 *      dispatch and its buffer copy, attempt 1 publishes TE, the retry's recording is never
 *      submitted, TC and TD are counted incomplete, nothing is unordered, and the deciding reuse
 *      (TC's buffer copy against TE's copy) is quoted. RED on the pre-fix source and both mutants.
 *   3c. the recovery control, scenario C -- the same boundary on scenario C: all four rectangles
 *      published exactly (TF on the first attempt, TX, TC2 and TD2 on the retry), exactly one
 *      retry, readback_incomplete 0, and across both passes every one of the 7 write/write reuses
 *      AND all 3 write/read pairs ordered -- including the abandoned read against the retry's own
 *      dispatches, which the submission-and-fence boundary orders. RED on the pre-fix source and
 *      both mutants (the abandoned read against TF's dispatch).
 *   4d. the permanent-failure form, scenario C -- TD2's set fails in attempt 1 (so the WAR pair IS
 *      recorded, unlike 4b's form) while TF's own set still succeeds and publishes, and every
 *      allocation from the retry's on fails: the boundary stops with TX, TC2 and TD2 counted
 *      incomplete, no frame advanced, no target stale, the WAR pair quoted and ordered. RED on the
 *      pre-fix source and both mutants.
 *   5. the success control              -- no injection: the three targets publish in exactly two
 *      batches (two recordings, submits, fence waits and invalidations), all three rectangles
 *      exact, zero unordered reuses, exactly one staging pair created and nothing freed.
 *
 * Expected verdicts
 * -----------------
 * The repaired source passes every property (14 verdicts, 275 checks). The same file with the
 * abandonment `vb_barrier();` deleted -- the recording this case exists to reject -- is measured to
 * FAIL 11 of the 14: properties 0, 1a, 1b, 1c, 1d, 3, 3b, 3c, 4, 4c and 4d, 31 of the 275 checks.
 * The write-after-write cases fail on the deciding check (an overlapping reuse with no barrier
 * between the two writes), 1d and 3c also on the write-after-READ check (no barrier covering the
 * reader's stage and the later writer's), and 0 on the marker the repair adds. 1b is RED there too:
 * its reuse is the abandoned group's staging copy against the next group's pack-output copy, and
 * the pack path's own barrier -- COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ -- orders
 * only that dispatch's write before the copy that reads its slice, so with the abandonment barrier
 * gone nothing names the pack-output copy's own write. It PASSES 2, 4b and 5 -- 4b because its
 * reuse is absent altogether (its failure precedes every write of the group), 2 and 5 because the
 * publication's bookkeeping (which target was published, from which slice) is not what the defect
 * breaks. Those three controls passing is the sanity anchor that separates "the defect" from "the
 * fixture".
 *
 * The mutant rows are in the report, and both mutants are RED on the same 11 properties: with the
 * call deleted outright, 31 of the 275 checks; with it replaced in place by a barrier whose
 * destination is VK_PIPELINE_STAGE_HOST_BIT with VK_ACCESS_HOST_READ_BIT, 29 of the 275. The
 * narrow-barrier mutant is the one that shows why the check is the recorded stream and not the
 * presence of a call: a HOST_READ destination and a HOST destination stage order neither of the two
 * arrows, so 1a and 1c are RED even though a barrier sits exactly where the repair's does (their
 * "exactly one barrier lies strictly between" checks pass there, and only the ordering does not),
 * and 1d/3c/4d are RED on the WAR check for that mutant too -- because the HOST destination stage
 * does not cover COMPUTE SHADER, which is what the write-after-read arrow needs.
 *
 * There is no skip mode and no SKIP_RETURN_CODE in the build: the seam is inside this translation
 * unit, so the case always runs, and anything it cannot do (a missing seam, a failed allocation)
 * is a FAILURE, exit 1.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */

#include "vk_readback_ordering_gate_seam.h"     /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture's geometry ─────────────────────────────────────────────────────────── */

/* The guest arena: the three targets' rectangles are contiguous from 0, and every rectangle
 * covers its target's whole guest surface, so "nothing outside the rectangles changed" is one
 * comparison of a small tail. */
#define T0_VA       0u
#define T0_PITCH    16384u
#define T0_W        4096u
#define T0_H        4096u
#define T0_SLICE    ((VkDeviceSize)T0_PITCH * T0_H)         /* 67108864 = 64 MiB */
#define T0_MARKER   0x10u

#define T1_VA       (T0_VA + (uint32_t)T0_SLICE)            /* 67108864 */
#define T1_PITCH    8192u
#define T1_W        4096u
#define T1_H        4096u
#define T1_SLICE    ((VkDeviceSize)T1_PITCH * T1_H)         /* 33554432 = 32 MiB */
#define T1_MARKER   0x20u

#define T2_VA       (T1_VA + (uint32_t)T1_SLICE)            /* 100663296 */
#define T2_PITCH    256u
#define T2_W        64u
#define T2_H        64u
#define T2_SLICE    ((VkDeviceSize)T2_PITCH * T2_H)         /* 16384 */
#define T2_MARKER   0x30u

/* T2's 16-bit variant: the same 64x64 rectangle, packed, 128-byte rows, an 8192-byte slice. */
#define T2P_PITCH   128u
#define T2P_W       64u
#define T2P_H       64u
#define T2P_SLICE   ((VkDeviceSize)T2P_PITCH * T2P_H)       /* 8192 */

/* ── Scenario B's geometry: the same three-target shape, one write class later ─────────────
 *
 * Scenario A's abandoned group records a 4-byte target's image copy as its last write. Scenario
 * B makes the abandoned group's last write the OTHER class the publication records: a 16-bit
 * target's pack-output buffer copy (g.pack_buf -> g.rb_buf). TC is the group's first staged
 * target, so its packed slice is at offset 0 and its pack set allocates; TD is the target whose
 * pack set is the injected failure, so the group is abandoned after TC's dispatch, TC's pack
 * barrier and TC's buffer copy were recorded; TE is the 4-byte target that does not fit and
 * starts the next group at slice offset 0 again, its own copy overwriting the bytes TC's
 * abandoned buffer copy wrote.
 *
 * The arena is shared with scenario A: TC at 0, TD after it, TE after TD, and TE's 64 MiB
 * rectangle ends exactly where A's third target's footprint ends. */
#define TC_VA       0u
#define TC_PITCH    8192u
#define TC_W        4096u
#define TC_H        4096u
#define TC_SLICE    ((VkDeviceSize)TC_PITCH * TC_H)         /* 33554432 = 32 MiB */
#define TC_MARKER   0xA0u

#define TD_VA       (TC_VA + (uint32_t)TC_SLICE)            /* 33554432 */
#define TD_PITCH    256u
#define TD_W        128u
#define TD_H        64u
#define TD_SLICE    ((VkDeviceSize)TD_PITCH * TD_H)         /* 16384 = 16 KiB */
#define TD_MARKER   0xB0u

#define TE_VA       (TD_VA + (uint32_t)TD_SLICE)            /* 33570816 */
#define TE_PITCH    16384u
#define TE_W        4096u
#define TE_H        4096u
#define TE_SLICE    ((VkDeviceSize)TE_PITCH * TE_H)         /* 67108864 = 64 MiB */
#define TE_MARKER   0xC0u

/* ── Scenario C's geometry: the abandoned recording's device PACK-BUFFER dependency ───────
 *
 * Scenario B's abandoned group's pack path reads and writes g.rb_buf. Scenario C makes the
 * abandoned recording end with a SUCCESSFUL pack path whose buffer copy READS g.pack_buf
 * [0, 32 MiB) -- and then has the next group's first target be 16-bit, so its own dispatch WRITES
 * g.pack_buf at offset 0 again: a write-after-read pair on the device pack buffer, which a
 * writes-only log cannot see at all.
 *
 * Group 1 = [TC2 16-bit 4096x4096 -> 32 MiB slice at offset 0, allocation 1 succeeds][TX 4-byte
 * 4096x2048 -> 32 MiB slice at 33554432][TD2 16-bit small -> allocation 2 FAILS, group abandoned]
 * = 64 MiB + 16 KiB. TC2 must be FIRST so its slice -- and therefore the pack buffer range its
 * copy reads -- is at offset 0; that is what makes the later dispatch's write of [0, ...) overlap
 * the abandoned read. Group 2 = [TF 16-bit 4096x4096 -> 32 MiB slice at offset 0], because
 * 64 MiB + 16 KiB + 32 MiB > 96 MiB: its dispatch writes g.pack_buf [0, 32 MiB), so the WAR pair
 * is between TC2's abandoned read and TF's own write.
 *
 * TC2's abandoned buffer copy also writes g.rb_buf [0, 32 MiB), which TF's own copy writes again:
 * the write-after-write class is decided here too. And the two 16-bit dispatches write the same
 * g.pack_buf bytes, across the abandoned group boundary: that class -- and the abandoned dispatch's
 * g.pack_buf write against the next group's commands in general -- is ordered by the abandonment
 * barrier, not by the pack path's own one, which is COMPUTE_SHADER/SHADER_WRITE ->
 * TRANSFER/TRANSFER_READ and orders a dispatch's write before the copy that reads that slice. The
 * fourth footprint ends exactly where the other scenarios' do. */
#define TC2_VA      0u                                      /* TC2 is staged FIRST, at offset 0 */
#define TC2_PITCH   8192u
#define TC2_W       4096u
#define TC2_H       4096u
#define TC2_SLICE   ((VkDeviceSize)TC2_PITCH * TC2_H)       /* 33554432 = 32 MiB */
#define TC2_MARKER  0xE0u

#define TX_VA       (TC2_VA + (uint32_t)TC2_SLICE)          /* 33554432 */
#define TX_PITCH    16384u
#define TX_W        4096u
#define TX_H        2048u
#define TX_SLICE    ((VkDeviceSize)TX_PITCH * TX_H)         /* 33554432 = 32 MiB */
#define TX_MARKER   0xD0u

#define TD2_VA      (TX_VA + (uint32_t)TX_SLICE)            /* 67108864 */
#define TD2_PITCH   256u
#define TD2_W       128u
#define TD2_H       64u
#define TD2_SLICE   ((VkDeviceSize)TD2_PITCH * TD2_H)       /* 16384 */
#define TD2_MARKER  0xF0u

#define TF_VA       (TD2_VA + (uint32_t)TD2_SLICE)          /* 67125248 */
#define TF_PITCH    8192u
#define TF_W        4096u
#define TF_H        4096u
#define TF_SLICE    ((VkDeviceSize)TF_PITCH * TF_H)         /* 33554432 = 32 MiB */
#define TF_MARKER   0x80u

/* T0 + T1 fill the staging cap exactly, so the third target's group starts at offset 0 again. */
#define GATE_CAP    ((VkDeviceSize)96u << 20)

/* The most targets one scenario drives (four, in scenario C: two in the abandoned group's
 * group, the one whose allocation fails, and the next group's own target). */
#define GATE_SCENARIO_MAX 4u

/* Where the watched tail starts: past the SHORTER of the two footprints the third target can
 * have in scenario A, so the window covers every byte of the arena the driven rectangles may not
 * touch in either scenario. */
#define TAIL_OFF    ((uint32_t)T2P_SLICE)
#define GATE_GUEST  ((size_t)(T2_VA + (uint32_t)T2_SLICE) + 4096u)
#define GATE_TAIL   ((size_t)(GATE_GUEST - (size_t)(T2_VA + TAIL_OFF)))

/* What vkMapMemory hands back: at least the cap, plus slack for the fixture's own bookkeeping. */
#define GATE_STAGE  ((size_t)T0_SLICE + (size_t)T1_SLICE + 65536u)

#define GATE_SEED   0x1D4C0000u
#define GATE_ROW    16384u                      /* the widest row either side compares */

/* ── The fixture's targets ──────────────────────────────────────────────────────────── */

typedef struct {
    const char *name;
    uint32_t va, pitch, bpp, w, h;
    VkDeviceSize slice;
    uint32_t marker;                /* what gate_world leaves in the target's rectangle */
    uint32_t stage;                 /* what the fixture supplies for the target's slice bytes */
} GateTarget;

/* The three targets scenario A drives, its 16-bit variant of the third, and scenario B's three.
 * `slice` is the fixture's expectation, checked against vb_readback_slice's own answer in
 * property 0; `marker` and `stage` are byte patterns that never equal each other, so "which
 * target's bytes are these" is an exact comparison. */
static const GateTarget s_tdef[11] = {
    { "T0 4-byte 4096x4096 pitch 16384", T0_VA, T0_PITCH, 4u, T0_W, T0_H, T0_SLICE, T0_MARKER,
      0x50u },
    { "T1 16-bit 4096x4096 pitch  8192", T1_VA, T1_PITCH, 2u, T1_W, T1_H, T1_SLICE, T1_MARKER,
      0x60u },
    { "T2 4-byte   64x64   pitch   256", T2_VA, T2_PITCH, 4u, T2_W, T2_H, T2_SLICE, T2_MARKER,
      0x70u },
    { "T2 16-bit   64x64   pitch   128", T2_VA, T2P_PITCH, 2u, T2P_W, T2P_H, T2P_SLICE, T2_MARKER,
      0x70u },
    { "TC 16-bit 4096x4096 pitch  8192", TC_VA, TC_PITCH, 2u, TC_W, TC_H, TC_SLICE, TC_MARKER,
      0x60u },
    { "TD 16-bit  128x64   pitch   256", TD_VA, TD_PITCH, 2u, TD_W, TD_H, TD_SLICE, TD_MARKER,
      0x70u },
    { "TE 4-byte 4096x4096 pitch 16384", TE_VA, TE_PITCH, 4u, TE_W, TE_H, TE_SLICE, TE_MARKER,
      0x50u },
    { "TC2 16-bit 4096x4096 pitch 8192", TC2_VA, TC2_PITCH, 2u, TC2_W, TC2_H, TC2_SLICE,
      TC2_MARKER, 0x60u },
    { "TX 4-byte 4096x2048 pitch 16384", TX_VA, TX_PITCH, 4u, TX_W, TX_H, TX_SLICE, TX_MARKER,
      0x50u },
    { "TD2 16-bit 128x64  pitch  256", TD2_VA, TD2_PITCH, 2u, TD2_W, TD2_H, TD2_SLICE, TD2_MARKER,
      0x70u },
    { "TF 16-bit 4096x4096 pitch 8192", TF_VA, TF_PITCH, 2u, TF_W, TF_H, TF_SLICE, TF_MARKER,
      0x90u },
};

static uint8_t *g_guest;                        /* xbox_GetMemoryOffset()'s answer */
static uint8_t *g_stage;                        /* what vkMapMemory hands back */
static uint8_t *g_row;                          /* one row of the fixture's own bytes */
static uint8_t g_tail[GATE_TAIL];               /* the arena's bytes outside the rectangles */
static uint8_t *g_stream[VB_SLOTS];             /* one mapped slot stream each, as install leaves it */

static uintptr_t g_handle = 0x8000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

/* ── The recorded command stream ────────────────────────────────────────────────────── */

#define GATE_MAX_WRITES 64
#define GATE_MAX_BARS   64
#define GATE_MAX_SUBS   16
#define GATE_MAX_BEGINS 16
#define GATE_NOSLOT     0xFFFFFFFFu

typedef struct {
    unsigned seq;                   /* recording order, one counter for writes/bars/submits */
    unsigned rec;                   /* which recording it was recorded into */
    unsigned slot;                  /* which ring slot's command buffer, or GATE_NOSLOT */
    VkCommandBuffer cb;             /* the command buffer it was recorded into */
    const char *how;                /* the entry point that recorded it */
    VkBuffer buf;                   /* the buffer written */
    VkDeviceSize off, size;         /* the range written */
    unsigned target;                /* the fixture target it belongs to, or GATE_NOSLOT */
    int recording;                  /* g.slots[slot].recording at that moment */
    unsigned npend;                 /* g.npending at that moment */
    VkPipelineStageFlags stage;     /* the stage that performs the write */
    VkAccessFlags access;           /* TRANSFER_WRITE, or SHADER_WRITE for the pack dispatch */
} RecWrite;

typedef struct {
    unsigned seq;
    VkCommandBuffer cb;
    unsigned rec;
    VkPipelineStageFlags src, dst;
    VkAccessFlags srca, dsta;
} RecBar;

/* A buffer READ a recorded command performs. The readback path's device read of a staging buffer
 * is the pack path's buffer copy, which reads the packed slice out of g.pack_buf; it is recorded
 * so the write-after-read class (WAR) can be decided on that buffer, which a writes-only log
 * cannot see at all. */
typedef struct {
    unsigned seq;                   /* recording order, in the same counter as the writes */
    unsigned rec;
    unsigned slot;
    VkCommandBuffer cb;
    const char *how;                /* the entry point that recorded it */
    VkBuffer buf;                   /* the buffer read */
    VkDeviceSize off, size;         /* the range read */
    unsigned target;                /* the fixture target it belongs to, or GATE_NOSLOT */
    VkPipelineStageFlags stage;     /* the stage that performs the read */
    VkAccessFlags access;           /* TRANSFER_READ */
} RecRead;

typedef struct {
    unsigned seq;                   /* when the submission was recorded */
    VkCommandBuffer cb;
    unsigned rec;                   /* the recording it ended */
    unsigned slot;
    unsigned waited_seq;            /* when a fence wait for it completed, 0 if never */
} RecSubmit;

typedef struct {
    unsigned rec;                   /* the recording that begins */
    unsigned seq;
    unsigned npending;              /* the ring's state at that moment */
    int pending0;                   /* slot 0's fence */
    int recording0;                 /* slot 0's recording bit */
    unsigned fence_resets, pool_resets;
} RecBegin;

/* Every Vulkan call the publication's paths can reach, and what it saw. */
typedef struct {
    unsigned begins;                /* vkBeginCommandBuffer */
    VkResult begin_result;
    VkCommandBuffer began;
    unsigned ends;                  /* vkEndCommandBuffer */
    VkCommandBuffer ended;
    unsigned resets;                /* vkResetCommandBuffer */
    VkCommandBuffer reset_cb;
    unsigned submits;               /* vkQueueSubmit */
    VkCommandBuffer submitted[GATE_MAX_SUBS];
    unsigned waits;                 /* vkWaitForFences */
    VkResult wait_result;
    unsigned fence_resets;          /* vkResetFences */
    unsigned pool_resets;           /* vkResetDescriptorPool */
    unsigned ds_allocs;             /* vkAllocateDescriptorSets */
    unsigned ds_failures;           /* allocations that returned the injected failure */
    unsigned ds_fail_at;            /* the 1-based call this fixture makes fail; 0: never */
    unsigned ds_fail_all;           /* 1: every allocation fails (property 4) */
    unsigned ds_fail_seq;           /* the sequence number of the last injected failure */
    unsigned ds_fail_seq_first;     /* ... and of the first one */
    VkResult ds_result;             /* what that failure returns */
    VkDescriptorPool ds_pool;       /* the pool the last allocation asked */
    unsigned updates;               /* vkUpdateDescriptorSets */
    unsigned pipebinds;             /* vkCmdBindPipeline */
    VkPipeline bound_pipe;
    VkPipelineBindPoint bind_point;
    unsigned setbinds;              /* vkCmdBindDescriptorSets */
    unsigned pushes;                /* vkCmdPushConstants */
    VbPackPush push;                /* the pack block, copied as the production recorded it */
    unsigned dispatches;            /* vkCmdDispatch */
    VkCommandBuffer dispatch_cb;
    uint32_t dispatch_group[3];
    unsigned imgcopy;               /* vkCmdCopyImageToBuffer */
    VkCommandBuffer imgcopy_cb;
    VkImage imgcopy_img;
    VkBuffer imgcopy_buf;
    VkImageLayout imgcopy_layout;
    VkBufferImageCopy imgcopy_region;
    unsigned bufcopy;               /* vkCmdCopyBuffer */
    VkCommandBuffer bufcopy_cb;
    VkBuffer bufcopy_src, bufcopy_dst;
    VkBufferCopy bufcopy_region;
    unsigned bars;                  /* vkCmdPipelineBarrier with a memory barrier */
    unsigned hostbars;              /* ... whose destination stage is HOST */
    VkCommandBuffer bar_cb;
    unsigned invalidates;           /* vkInvalidateMappedMemoryRanges */
    VkMappedMemoryRange invalidate_range;
    unsigned buffers;               /* vkCreateBuffer */
    unsigned buffer_destroys;       /* vkDestroyBuffer */
    unsigned allocs;                /* vkAllocateMemory */
    unsigned frees;                 /* vkFreeMemory */
    unsigned maps;                  /* vkMapMemory */
    unsigned ds_fail_from;          /* fail every allocation from this call on; 0: off */

    /* What the fixture supplied, as the device executing the last submitted recording. */
    unsigned supply_calls;          /* invalidations that supplied a batch's bytes */
    unsigned supply_writes;         /* writes supplied over all of them */
    unsigned supply_target;         /* the last write supplied: which target */
    VkDeviceSize supply_off, supply_size;

    /* The stream, in recording order. */
    unsigned seq;                   /* the shared sequence counter */
    unsigned rec;                   /* the recording counter */
    unsigned slot_rec[VB_SLOTS];    /* the recording each slot's command buffer holds */
    RecWrite writes[GATE_MAX_WRITES];
    unsigned nwrites;
    RecRead reads[GATE_MAX_WRITES];
    unsigned nreads;
    RecBar bars_rec[GATE_MAX_BARS];
    unsigned nbars;
    RecSubmit subs[GATE_MAX_SUBS];
    unsigned nsubs;
    unsigned wait_seq[GATE_MAX_SUBS];
    unsigned nwaits;
    RecBegin beg[GATE_MAX_BEGINS];
    unsigned nbegins;
    /* The fixture's own handles, so a write can be read as the target's own bytes: the image a
     * copy reads, the view the pack path binds in its descriptor set (which is how the pack
     * dispatch and its buffer copy are attributed to a target), and the bytes the fixture
     * supplies for that target's slice. */
    VkImage tgt_img[GATE_SCENARIO_MAX];
    VkImageView tgt_view[GATE_SCENARIO_MAX];
    uint32_t tgt_bpp[GATE_SCENARIO_MAX];
    uint32_t tgt_stage[GATE_SCENARIO_MAX];
    unsigned pack_target;           /* the target the pack path is currently packing */
} GateCtl;

static GateCtl c;

static unsigned gate_slot_of(VkCommandBuffer cb)
{
    unsigned i;

    for (i = 0; i < VB_SLOTS; i++)
        if (g.slots[i].cmd == cb)
            return i;
    return GATE_NOSLOT;
}

static unsigned gate_rec_of(VkCommandBuffer cb)
{
    unsigned i = gate_slot_of(cb);

    return i == GATE_NOSLOT ? 0u : c.slot_rec[i];
}

static unsigned gate_target_of_image(VkImage img)
{
    unsigned i;

    for (i = 0; i < GATE_SCENARIO_MAX; i++)
        if (c.tgt_img[i] == img)
            return i;
    return GATE_NOSLOT;
}

static uint32_t gate_bpp_of_image(VkImage img)
{
    unsigned i = gate_target_of_image(img);

    return i == GATE_NOSLOT ? 4u : c.tgt_bpp[i];
}

/* Record one write: which buffer, which range, in which command buffer, by which stage and which
 * access -- plus where the ring was when it was recorded. */
static void gate_record_write(const char *how, VkBuffer buf, VkDeviceSize off, VkDeviceSize size,
                              VkCommandBuffer cb, unsigned target, VkPipelineStageFlags stage,
                              VkAccessFlags access)
{
    RecWrite *w;
    unsigned slot = gate_slot_of(cb);

    if (c.nwrites >= GATE_MAX_WRITES) {
        fprintf(stderr, "FAIL: vk_readback_ordering_gate: the write log overflowed (%u)\n",
                GATE_MAX_WRITES);
        return;
    }
    w = &c.writes[c.nwrites++];
    w->seq = ++c.seq;
    w->cb = cb;
    w->slot = slot;
    w->rec = slot == GATE_NOSLOT ? 0u : c.slot_rec[slot];
    w->how = how;
    w->buf = buf;
    w->off = off;
    w->size = size;
    w->target = target;
    w->recording = slot == GATE_NOSLOT ? -1 : g.slots[slot].recording;
    w->npend = g.npending;
    w->stage = stage;
    w->access = access;
}

/* Record one buffer read: which buffer, which range, in which command buffer, by which stage. */
static void gate_record_read(const char *how, VkBuffer buf, VkDeviceSize off, VkDeviceSize size,
                             VkCommandBuffer cb, unsigned target, VkPipelineStageFlags stage,
                             VkAccessFlags access)
{
    RecRead *r;
    unsigned slot = gate_slot_of(cb);

    if (c.nreads >= GATE_MAX_WRITES) {
        fprintf(stderr, "FAIL: vk_readback_ordering_gate: the read log overflowed (%u)\n",
                GATE_MAX_WRITES);
        return;
    }
    r = &c.reads[c.nreads++];
    r->seq = ++c.seq;
    r->cb = cb;
    r->slot = slot;
    r->rec = slot == GATE_NOSLOT ? 0u : c.slot_rec[slot];
    r->how = how;
    r->buf = buf;
    r->off = off;
    r->size = size;
    r->target = target;
    r->stage = stage;
    r->access = access;
}

static void gate_record_barrier(VkCommandBuffer cb, VkPipelineStageFlags src,
                                VkPipelineStageFlags dst, VkAccessFlags srca, VkAccessFlags dsta)
{
    RecBar *b;

    if (c.nbars >= GATE_MAX_BARS)
        return;
    b = &c.bars_rec[c.nbars++];
    b->seq = ++c.seq;
    b->cb = cb;
    b->rec = gate_rec_of(cb);
    b->src = src;
    b->dst = dst;
    b->srca = srca;
    b->dsta = dsta;
}

/* ── The stubs ──────────────────────────────────────────────────────────────────────── */

/* Defined with the rest of the fixture state: the bytes the device would have written for a
 * submitted batch, supplied when that batch's fence wait is behind it. */
static void gate_supply(void);

static VkResult VKAPI_CALL gate_begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info)
{
    unsigned slot = gate_slot_of(cmd);

    (void)info;
    c.begins++;
    c.began = cmd;
    c.slot_rec[slot == GATE_NOSLOT ? 0u : slot] = ++c.rec;
    if (c.nbegins < GATE_MAX_BEGINS) {
        RecBegin *b = &c.beg[c.nbegins++];

        b->rec = c.rec;
        b->seq = ++c.seq;
        b->npending = g.npending;
        b->pending0 = g.slots[0].pending;
        b->recording0 = g.slots[0].recording;
        b->fence_resets = c.fence_resets;
        b->pool_resets = c.pool_resets;
    }
    return c.begin_result;
}

static VkResult VKAPI_CALL gate_end(VkCommandBuffer cmd)
{
    c.ends++;
    c.ended = cmd;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_cmd_reset(VkCommandBuffer cmd, VkCommandBufferResetFlags flags)
{
    (void)flags;
    c.resets++;
    c.reset_cb = cmd;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits,
                                       VkFence fence)
{
    VkCommandBuffer cb = VK_NULL_HANDLE;

    (void)queue; (void)fence;
    c.submits++;
    if (count && submits[0].commandBufferCount)
        cb = submits[0].pCommandBuffers[0];
    if (c.submits <= GATE_MAX_SUBS)
        c.submitted[c.submits - 1u] = cb;
    if (c.nsubs < GATE_MAX_SUBS) {
        RecSubmit *s = &c.subs[c.nsubs++];

        s->seq = ++c.seq;
        s->cb = cb;
        s->rec = gate_rec_of(cb);
        s->slot = gate_slot_of(cb);
        s->waited_seq = 0;
    }
    return VK_SUCCESS;
}

/* A fence wait: the ring's fences are one per slot, so the submission it completes is that
 * slot's latest unwaited one. That is the boundary a later recording is ordered by. */
static VkResult VKAPI_CALL gate_wait(VkDevice device, uint32_t count, const VkFence *fences,
                                     VkBool32 all, uint64_t timeout)
{
    unsigned i;

    (void)device; (void)all; (void)timeout;
    c.waits++;
    if (count && fences) {
        for (i = 0; i < VB_SLOTS; i++)
            if (g.slots[i].fence == fences[0]) {
                unsigned k;

                for (k = c.nsubs; k > 0u; k--)
                    if (c.subs[k - 1u].slot == i && !c.subs[k - 1u].waited_seq) {
                        c.subs[k - 1u].waited_seq = c.seq + 1u;
                        break;
                    }
                break;
            }
    }
    if (c.nwaits < GATE_MAX_SUBS)
        c.wait_seq[c.nwaits] = ++c.seq;
    c.nwaits++;
    return c.wait_result;
}

static VkResult VKAPI_CALL gate_fence_reset(VkDevice device, uint32_t count, const VkFence *fences)
{
    (void)device; (void)count; (void)fences;
    c.fence_resets++;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_pool_reset(VkDevice device, VkDescriptorPool pool,
                                           VkDescriptorPoolResetFlags flags)
{
    (void)device; (void)pool; (void)flags;
    c.pool_resets++;
    return VK_SUCCESS;
}

/* The one outcome this case injects: a pack descriptor set cannot be allocated. Either exactly
 * once at a chosen call, on every call, or from a chosen call on (the failure persisting, which
 * is the permanent-failure form that still records the abandoned group's pack writes). */
static VkResult VKAPI_CALL gate_ds_alloc(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                         VkDescriptorSet *set)
{
    (void)device;
    c.ds_allocs++;
    c.ds_pool = info ? info->descriptorPool : VK_NULL_HANDLE;
    if (c.ds_fail_all || (c.ds_fail_at && c.ds_allocs == c.ds_fail_at)
        || (c.ds_fail_from && c.ds_allocs >= c.ds_fail_from)) {
        c.ds_failures++;
        if (c.ds_failures == 1u)
            c.ds_fail_seq_first = ++c.seq;
        c.ds_fail_seq = ++c.seq;
        *set = VK_NULL_HANDLE;
        return c.ds_result;
    }
    *set = (VkDescriptorSet)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_ds_update(VkDevice device, uint32_t writes,
                                      const VkWriteDescriptorSet *w, uint32_t copies,
                                      const VkCopyDescriptorSet *cp)
{
    unsigned i, k;

    (void)device; (void)copies; (void)cp;
    c.updates++;
    /* The pack path binds the target's OWN view in binding 0, which is how its dispatch and the
     * buffer copy that reads the pack buffer are attributed to the target they pack. */
    for (i = 0; w && i < writes; i++)
        if (w[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && w[i].pImageInfo)
            for (k = 0; k < GATE_SCENARIO_MAX; k++)
                if (c.tgt_view[k] == w[i].pImageInfo[0].imageView)
                    c.pack_target = k;
}

static void VKAPI_CALL gate_bind_pipeline(VkCommandBuffer cmd, VkPipelineBindPoint point,
                                          VkPipeline pipe)
{
    (void)cmd;
    c.pipebinds++;
    c.bind_point = point;
    c.bound_pipe = pipe;
}

static void VKAPI_CALL gate_bind_sets(VkCommandBuffer cmd, VkPipelineBindPoint point,
                                      VkPipelineLayout layout, uint32_t first, uint32_t count,
                                      const VkDescriptorSet *sets, uint32_t dyn_count,
                                      const uint32_t *dyn)
{
    (void)cmd; (void)point; (void)layout; (void)first; (void)count; (void)sets; (void)dyn_count;
    (void)dyn;
    c.setbinds++;
}

static void VKAPI_CALL gate_push_constants(VkCommandBuffer cmd, VkPipelineLayout layout,
                                           VkShaderStageFlags stage, uint32_t offset,
                                           uint32_t size, const void *values)
{
    (void)cmd; (void)layout; (void)stage; (void)offset;
    c.pushes++;
    memset(&c.push, 0, sizeof c.push);
    if (values && size)
        memcpy(&c.push, values, size < sizeof c.push ? size : sizeof c.push);
}

/* The pack kernel's write: the dispatch stores the packed rectangle into the device-local pack
 * buffer, at the byte offset and row stride its push block carries. */
static void VKAPI_CALL gate_dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z)
{
    VkDeviceSize off = (VkDeviceSize)c.push.dst[0];
    VkDeviceSize size = (VkDeviceSize)c.push.dst[1] * (VkDeviceSize)c.push.rect[3];

    c.dispatches++;
    c.dispatch_cb = cmd;
    c.dispatch_group[0] = x;
    c.dispatch_group[1] = y;
    c.dispatch_group[2] = z;
    gate_record_write("vkCmdDispatch (pack kernel, shader writes)", g.pack_buf, off, size, cmd,
                      c.pack_target, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_ACCESS_SHADER_WRITE_BIT);
}

static void VKAPI_CALL gate_copy_to_buffer(VkCommandBuffer cmd, VkImage src,
                                           VkImageLayout layout, VkBuffer dst, uint32_t count,
                                           const VkBufferImageCopy *regions)
{
    unsigned target = gate_target_of_image(src);

    c.imgcopy++;
    c.imgcopy_cb = cmd;
    c.imgcopy_img = src;
    c.imgcopy_layout = layout;
    c.imgcopy_buf = dst;
    if (count) {
        VkDeviceSize size;

        c.imgcopy_region = regions[0];
        /* The slice's bytes: the region's row length in pixels, the target's own bytes per
         * pixel, and the rows it copies. */
        size = (VkDeviceSize)regions[0].bufferRowLength * (VkDeviceSize)gate_bpp_of_image(src)
               * (VkDeviceSize)regions[0].imageExtent.height;
        gate_record_write("vkCmdCopyImageToBuffer", dst, regions[0].bufferOffset, size, cmd,
                          target, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    }
}

static void VKAPI_CALL gate_copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
                                        uint32_t count, const VkBufferCopy *regions)
{
    c.bufcopy++;
    c.bufcopy_cb = cmd;
    c.bufcopy_src = src;
    c.bufcopy_dst = dst;
    if (count) {
        c.bufcopy_region = regions[0];
        /* The read first: the copy reads the packed slice out of the pack buffer, and that read is
         * what the WAR class is decided on. */
        gate_record_read("vkCmdCopyBuffer (pack output read)", src, regions[0].srcOffset,
                         regions[0].size, cmd, c.pack_target, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT);
        gate_record_write("vkCmdCopyBuffer", dst, regions[0].dstOffset, regions[0].size, cmd,
                          c.pack_target, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT);
    }
}

static void VKAPI_CALL gate_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
                                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                                    uint32_t mbc, const VkMemoryBarrier *mb, uint32_t bbc,
                                    const VkBufferMemoryBarrier *bb, uint32_t ibc,
                                    const VkImageMemoryBarrier *ib)
{
    (void)flags; (void)bb; (void)ib;
    if (!(mbc && mb))
        return;                                 /* this fixture commands no image barriers */
    c.bars++;
    c.bar_cb = cmd;
    if (dst & VK_PIPELINE_STAGE_HOST_BIT)
        c.hostbars++;
    gate_record_barrier(cmd, src, dst, mb[0].srcAccessMask, mb[0].dstAccessMask);
    (void)ibc;
}

static VkResult VKAPI_CALL gate_invalidate(VkDevice device, uint32_t count,
                                           const VkMappedMemoryRange *ranges)
{
    (void)device;
    gate_supply();                      /* the device has executed this batch's copies */
    c.invalidates++;
    if (count)
        c.invalidate_range = ranges[0];
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_end_pass(VkCommandBuffer cmd)
{
    (void)cmd;
}

static VkResult VKAPI_CALL gate_create_buffer(VkDevice device, const VkBufferCreateInfo *info,
                                              const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)info; (void)alloc;
    c.buffers++;
    *buf = (VkBuffer)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_buffer_requirements(VkDevice device, VkBuffer buf,
                                                VkMemoryRequirements *req)
{
    (void)device; (void)buf;
    req->size = 64u;
    req->alignment = 16u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_bind_buffer(VkDevice device, VkBuffer buf, VkDeviceMemory mem,
                                            VkDeviceSize offset)
{
    (void)device; (void)buf; (void)mem; (void)offset;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_destroy_buffer(VkDevice device, VkBuffer buf,
                                           const VkAllocationCallbacks *alloc)
{
    (void)device; (void)buf; (void)alloc;
    c.buffer_destroys++;
}

static VkResult VKAPI_CALL gate_allocate(VkDevice device, const VkMemoryAllocateInfo *info,
                                         const VkAllocationCallbacks *alloc, VkDeviceMemory *mem)
{
    (void)device; (void)info; (void)alloc;
    c.allocs++;
    *mem = (VkDeviceMemory)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_free(VkDevice device, VkDeviceMemory mem,
                                 const VkAllocationCallbacks *alloc)
{
    (void)device; (void)mem; (void)alloc;
    c.frees++;
}

/* The staging buffer the host publishes from: the same stand-in arena every time, so the test
 * can fill it with the bytes the GPU would have written there. */
static VkResult VKAPI_CALL gate_map(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    (void)device; (void)mem; (void)offset; (void)size; (void)flags;
    c.maps++;
    *ppData = g_stage;
    return VK_SUCCESS;
}

/* Point the back end's own entry points at the stubs above. The pointers are static in the
 * translation unit, which is why the file is compiled here. */
static void gate_wire(void)
{
    vkBeginCommandBuffer = gate_begin;
    vkEndCommandBuffer = gate_end;
    vkResetCommandBuffer = gate_cmd_reset;
    vkQueueSubmit = gate_submit;
    vkWaitForFences = gate_wait;
    vkResetFences = gate_fence_reset;
    vkResetDescriptorPool = gate_pool_reset;
    vkAllocateDescriptorSets = gate_ds_alloc;
    vkUpdateDescriptorSets = gate_ds_update;
    vkCmdBindPipeline = gate_bind_pipeline;
    vkCmdBindDescriptorSets = gate_bind_sets;
    vkCmdPushConstants = gate_push_constants;
    vkCmdDispatch = gate_dispatch;
    vkCmdCopyImageToBuffer = gate_copy_to_buffer;
    vkCmdCopyBuffer = gate_copy_buffer;
    vkCmdPipelineBarrier = gate_barrier;
    vkInvalidateMappedMemoryRanges = gate_invalidate;
    vkCmdEndRenderPass = gate_end_pass;
    vkCreateBuffer = gate_create_buffer;
    vkGetBufferMemoryRequirements = gate_buffer_requirements;
    vkBindBufferMemory = gate_bind_buffer;
    vkDestroyBuffer = gate_destroy_buffer;
    vkAllocateMemory = gate_allocate;
    vkFreeMemory = gate_free;
    vkMapMemory = gate_map;
}

/* ── Reporting ──────────────────────────────────────────────────────────────────────── */

static int g_checks, g_check_failures, g_props, g_prop_failures;
static unsigned g_reuses, g_unordered, g_war_reuses, g_war_unordered;

static void section(const char *title)
{
    printf("\n== %s ==\n", title);
    fflush(stdout);
}

static void verdict(const char *prop, int checks_before, int failures_before)
{
    int bad = g_check_failures - failures_before;
    g_props++;
    if (bad)
        g_prop_failures++;
    printf("  --> %-58s %s (%d checks, %d failed)\n", prop, bad ? "FAIL" : "PASS",
           g_checks - checks_before, bad);
    fflush(stdout);
}

static void check(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    g_checks++;
    if (!cond)
        g_check_failures++;
    printf("      %-66s %s", what, cond ? "ok" : "FAIL");
    if (fmt && fmt[0]) {
        printf("  (");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf(")");
    }
    putchar('\n');
    fflush(stdout);
}

static void note(const char *fmt, ...)
{
    va_list ap;
    printf("      note: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
}

static void marker(const char *what, int present)
{
    printf("      %-66s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

/* ── Fixture state ──────────────────────────────────────────────────────────────────── */

static void fill_bytes(uint8_t *p, size_t n, uint32_t seed)
{
    uint32_t w = 0x9E3779B9u ^ seed;
    size_t i;

    for (i = 0; i + 4u <= n; i += 4u) {
        memcpy(p + i, &w, 4);
        w = w * 1664525u + 1013904223u;
    }
    for (; i < n; i++)
        p[i] = (uint8_t)(w >> 24);
}

static uint64_t hash_bytes(const uint8_t *p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    size_t i;

    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

static size_t count_diff(const uint8_t *a, const uint8_t *b, size_t n)
{
    size_t i, bad = 0;

    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            bad++;
    return bad;
}

static void stats(Nv2aVkReadbackStats *out)
{
    memset(out, 0, sizeof *out);
    nv2a_vk_backend_readback_stats(out);
}

/* A staging-buffer pattern whose bytes never equal the guest marker's: an unwritten staging
 * buffer holds one (0x8x), what the fixture's "GPU" wrote holds another (0x5x/0x6x/0x7x), and the
 * guest's rectangles hold a third (0x1x/0x2x/0x3x). A pattern is a function of the byte's offset
 * within its row, so any row of it can be regenerated without keeping a copy. */
static void row_pattern(uint8_t *p, size_t n, uint32_t base)
{
    size_t i;

    for (i = 0; i < n; i++)
        p[i] = (uint8_t)(base + (i & 0x0Fu));
}

static void fill_stage_slice(uint8_t *p, size_t n, uint32_t base)
{
    size_t i;

    for (i = 0; i < n; i++)
        p[i] = (uint8_t)(base + (i & 0x0Fu));
}

/* ── The GPU's half of each batch: the bytes its copies would have written ─────────────────
 *
 * The fixture stands in for the copy engine and the pack kernel, and it supplies those bytes AT
 * THE BATCH'S FENCE WAIT -- the invalidation the publication performs immediately before it
 * memcpys them into guest memory -- rather than up front. That is not a detail: two batches'
 * slices share bytes, because both start at their own group's slice offset 0, so a fill made
 * once for everything would leave one batch's pattern at the other batch's offsets and no
 * published rectangle could be compared with its OWN image. Supplying them here, for the
 * recording the last submission ended, is what a real device does: those bytes exist once that
 * batch has executed.
 *
 * Which order the device picked for an unordered pair is NOT modelled -- the fixture supplies
 * the writes in recording order, the benign one -- and nothing here decides on the bytes: the
 * case decides the RECORDED STREAM (see the ordering decision below). */
static void gate_supply(void)
{
    unsigned i;

    if (!c.nsubs)
        return;
    for (i = 0; i < c.nwrites; i++) {
        const RecWrite *w = &c.writes[i];

        if (w->rec != c.subs[c.nsubs - 1u].rec || w->buf != g.rb_buf
            || w->target >= GATE_SCENARIO_MAX)
            continue;
        fill_stage_slice(g_stage + w->off, (size_t)w->size, c.tgt_stage[w->target]);
        c.supply_writes++;
        c.supply_target = w->target;
        c.supply_off = w->off;
        c.supply_size = w->size;
    }
    c.supply_calls++;
}

/* The whole back end's state as install/first use leaves it, with the ring idle, the guest arena
 * seeded and the driven targets' rectangles marked. `tail_va` is where the arena's bytes outside
 * every driven rectangle begin; the arena is watched from there to its end. */
static void gate_world(const GateTarget *const *def, unsigned ndef, uint32_t tail_va)
{
    unsigned i, k;

    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    c.begin_result = VK_SUCCESS;
    c.wait_result = VK_SUCCESS;
    c.ds_result = VK_ERROR_OUT_OF_POOL_MEMORY;
    c.ds_fail_at = 0;
    c.ds_fail_all = 0;
    c.ds_fail_from = 0;

    g.dev = (VkDevice)hnew();
    g.queue = (VkQueue)hnew();
    /* One memory type that satisfies every request, so vb_alloc's probes always land. */
    g.mp.memoryTypeCount = 1;
    g.mp.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    g.mp.memoryTypes[0].heapIndex = 0;
    g.ok = 1;
    g.dsl_pack = (VkDescriptorSetLayout)hnew();
    g.layout_pack = (VkPipelineLayout)hnew();
    g.pipe_pack = (VkPipeline)hnew();
    for (i = 0; i < VB_SLOTS; i++) {
        g.slots[i].cmd = (VkCommandBuffer)hnew();
        g.slots[i].fence = (VkFence)hnew();
        g.slots[i].dpool = (VkDescriptorPool)hnew();
        g.slots[i].stream = (VkBuffer)hnew();
        g.slots[i].stream_ptr = g_stream[i];
        fill_bytes(g_stream[i], VB_SLOT_STREAM, GATE_SEED + i);
    }
    fill_bytes(g_guest, GATE_GUEST, GATE_SEED + 7u);
    fill_stage_slice(g_stage, GATE_STAGE, 0x80u);   /* nothing has written the staging yet */
    /* The driven targets' rectangles, each with its own marker, so "which rectangle did this
     * publish touch" is an exact byte count; every footprint ends where the next one begins. */
    for (k = 0; k < ndef; k++) {
        const GateTarget *d = def[k];
        uint32_t row;

        for (row = 0; row < d->h; row++) {
            row_pattern(g_row, d->pitch, d->marker);
            memcpy(g_guest + d->va + (size_t)row * d->pitch, g_row, d->pitch);
        }
    }
    memcpy(g_tail, g_guest + tail_va, GATE_GUEST - (size_t)tail_va);
}

/* One target, wired the way the back end's own target path leaves it: production-legal geometry,
 * the dirty rectangle = the whole surface, and its own image and view. */
static void wire_target(VbTarget *t, const GateTarget *d, unsigned idx)
{
    memset(t, 0, sizeof *t);
    t->used = 1;
    t->va = d->va;
    t->pitch = d->pitch;
    t->bpp = d->bpp;
    t->w = d->w;
    t->h = d->h;
    t->dirty = 1;
    t->dx0 = 0;
    t->dy0 = 0;
    t->dx1 = d->w;
    t->dy1 = d->h;
    t->im.img = (VkImage)hnew();
    t->im.view = (VkImageView)hnew();
    c.tgt_img[idx] = t->im.img;
    c.tgt_view[idx] = t->im.view;
    c.tgt_bpp[idx] = d->bpp;
    c.tgt_stage[idx] = d->stage;
}

typedef struct {
    GateTarget g[GATE_SCENARIO_MAX];    /* what was wired */
    VbTarget *t[GATE_SCENARIO_MAX];
    VkDeviceSize off[GATE_SCENARIO_MAX];/* where the production's grouping puts each slice */
    uint32_t tail_va;                   /* where the arena's outside bytes start */
    size_t tail_len;
    unsigned n;
} Scenario;

/* Every scenario is the same shape, and gets there the same way: some dirty targets fill a group
 * (the last of them the one whose pack set is the injected failure, or none), and the target after
 * them does not fit, so it starts the NEXT group with slice offset 0 again. `off` is the slice
 * offset the production's own grouping gives each target: the first staged target of a group is at
 * 0 and each further one after the sum of the slices before it, which the case then checks against
 * what was actually recorded. */
static void scenario_world(Scenario *s, const GateTarget *const *def, unsigned n,
                           const VkDeviceSize *off, uint32_t tail_va)
{
    unsigned i;

    gate_world(def, n, tail_va);
    for (i = 0; i < n; i++) {
        s->g[i] = *def[i];
        wire_target(&g.targets[i], &s->g[i], i);
        s->t[i] = &g.targets[i];
        s->off[i] = off[i];
    }
    s->n = n;
    s->tail_va = tail_va;
    s->tail_len = GATE_GUEST - (size_t)tail_va;
}

/* Scenario A: T0 (4-byte, 64 MiB), T1 (16-bit, 32 MiB) fill the cap, T2 is the next group. The
 * third target is either 4-byte (its own copy is the reuse) or the small 16-bit variant (whose
 * pack output is). */
static void scenario(Scenario *s, int small16)
{
    const GateTarget *t2 = &s_tdef[small16 ? 3u : 2u];
    const GateTarget *def[3];
    const VkDeviceSize off[3] = { 0, T0_SLICE, 0 };

    def[0] = &s_tdef[0];
    def[1] = &s_tdef[1];
    def[2] = t2;
    scenario_world(s, def, 3u, off, t2->va + (uint32_t)t2->slice);
}

/* Scenario B: TC (16-bit, 32 MiB) and TD (16-bit, 16 KiB) fill a group -- TD's pack set is the
 * injected failure -- and TE (4-byte, 64 MiB) is the next group, at slice offset 0 again. */
static void scenario_b(Scenario *s)
{
    const GateTarget *def[3];
    const VkDeviceSize off[3] = { 0, TC_SLICE, 0 };

    def[0] = &s_tdef[4];
    def[1] = &s_tdef[5];
    def[2] = &s_tdef[6];
    scenario_world(s, def, 3u, off, def[2]->va + (uint32_t)def[2]->slice);
}

/* Scenario C: TC2 (16-bit, 32 MiB, the group's FIRST staged target so its slice -- and the pack
 * buffer range its copy reads -- is at offset 0) and TX (4-byte, 32 MiB) fill a group -- TD2's
 * pack set is the injected failure, so the group is abandoned with TC2's pack path recorded --
 * and TF (16-bit, 32 MiB) is the next group, at slice offset 0 again, so its dispatch writes the
 * pack buffer bytes the abandoned copy read. */
static void scenario_c(Scenario *s)
{
    const GateTarget *def[4];
    const VkDeviceSize off[4] = { 0, TC2_SLICE, TC2_SLICE + TX_SLICE, 0 };

    def[0] = &s_tdef[7];                /* TC2, staged first */
    def[1] = &s_tdef[8];                /* TX */
    def[2] = &s_tdef[9];                /* TD2, whose pack set fails */
    def[3] = &s_tdef[10];               /* TF, the next group */
    scenario_world(s, def, 4u, off, def[3]->va + (uint32_t)def[3]->slice);
}

/* What the fixture expects guest memory to hold for one target: exactly the bytes it supplied for
 * that target's slice -- the pattern of that target's OWN stage base over the whole slice, which
 * is what the batch that published it read. Comparing against the pattern rather than against the
 * staging buffer as it stands at the end is not a convenience: each batch's bytes are supplied
 * when THAT batch passes its fence, and a later batch legitimately rewrites the same offsets (its
 * slices start at its own group's offset 0), so the live buffer is the wrong yardstick for an
 * earlier publication. It is also the stronger statement: the rectangle holds that target's own
 * bytes, not merely whatever the arena happened to contain. Returns the number of bytes that
 * differ. */
static size_t published_bad(const Scenario *s, unsigned k)
{
    const uint8_t *have = g_guest + s->g[k].va;
    size_t n = (size_t)s->g[k].slice;
    size_t off, bad = 0;

    row_pattern(g_row, GATE_ROW, s->g[k].stage);    /* every 16 bytes repeat, so one row serves all */
    for (off = 0; off < n; off += GATE_ROW) {
        size_t chunk = n - off < (size_t)GATE_ROW ? n - off : (size_t)GATE_ROW;

        if (memcmp(have + off, g_row, chunk))
            bad += count_diff(have + off, g_row, chunk);
    }
    return bad;
}

/* What the fixture expects a target's rectangle to be if nothing has published it: the marker
 * pattern gate_world left there. */
static size_t marker_bad(const Scenario *s, unsigned k)
{
    const GateTarget *d = &s->g[k];
    size_t bad = 0;
    uint32_t row;

    for (row = 0; row < d->h; row++) {
        const uint8_t *have = g_guest + d->va + (size_t)row * d->pitch;

        row_pattern(g_row, d->pitch, d->marker);
        if (memcmp(have, g_row, d->pitch))
            bad += count_diff(have, g_row, d->pitch);
    }
    return bad;
}

/* The arena's bytes outside every driven rectangle, from where the scenario says they start. */
static size_t tail_bad(const Scenario *s)
{
    const uint8_t *have = g_guest + s->tail_va;

    return memcmp(have, g_tail, s->tail_len) ? count_diff(have, g_tail, s->tail_len) : 0u;
}

/* ── The ordering decision ──────────────────────────────────────────────────────────── */

typedef struct {
    unsigned reuses, unordered;         /* overlapping write/write pairs, and unordered ones */
    unsigned war_reuses, war_unordered; /* overlapping read/write pairs, and unordered ones */
} GateOrder;

/* VK_ACCESS_MEMORY_WRITE_BIT is the spec's "every write" bit and VK_ACCESS_MEMORY_READ_BIT its
 * "every read": a barrier that names one of them covers any access of that kind. */
#define GATE_WRITE_BITS (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT |          \
                         VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT |              \
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |                               \
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

static int gate_has_write(VkAccessFlags mask)
{
    return (mask & GATE_WRITE_BITS) != 0;
}

static int gate_access_covers(VkAccessFlags mask, VkAccessFlags access)
{
    if (mask & access)
        return 1;
    if ((mask & VK_ACCESS_MEMORY_WRITE_BIT) && gate_has_write(access))
        return 1;
    return 0;
}

static int gate_stage_covers(VkPipelineStageFlags mask, VkPipelineStageFlags stage)
{
    return (mask & stage) != 0 || (mask & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) != 0;
}

static int gate_overlap(const RecWrite *a, const RecWrite *b)
{
    return a->off < b->off + b->size && b->off < a->off + a->size;
}

/* The barrier strictly between the two writes that orders the later one after the earlier one,
 * or NULL. A barrier recorded after both does not count. Both access scopes must cover: the
 * barrier's source access its earlier writer's access, and its destination access the LATER
 * writer's access -- Vulkan makes the later access wait on the barrier's second half, so a
 * destination access that does not name the later write (a host READ, say) does not order two
 * device writes no matter what it names for the earlier one. */
static const RecBar *gate_ordering_barrier(const RecWrite *a, const RecWrite *b)
{
    unsigned i;

    for (i = 0; i < c.nbars; i++) {
        const RecBar *r = &c.bars_rec[i];

        if (r->rec != a->rec || r->seq <= a->seq || r->seq >= b->seq)
            continue;
        if (!gate_stage_covers(r->src, a->stage))
            continue;
        if (!gate_access_covers(r->srca, a->access))
            continue;
        if (!gate_stage_covers(r->dst, b->stage))
            continue;
        if (!gate_access_covers(r->dsta, b->access))
            continue;
        return r;
    }
    return NULL;
}

/* The barrier strictly between an earlier READ and a later WRITE that orders the write after the
 * read, or NULL. This is the write-after-read class (WAR), and what it needs is an EXECUTION
 * dependency: the later write must not start until the earlier read has finished, so the
 * barrier's source stage must cover the READER's stage and its destination stage the LATER
 * WRITER's stage. No access mask is consulted -- in particular the destination access is NOT
 * required to name anything, because a read does not become "available" the way a write does, and
 * requiring a destination access here would be exactly the mistake this gate exists to catch,
 * made in the other direction. */
static const RecBar *gate_ordering_barrier_war(const RecRead *a, const RecWrite *b)
{
    unsigned i;

    for (i = 0; i < c.nbars; i++) {
        const RecBar *r = &c.bars_rec[i];

        if (r->rec != a->rec || r->seq <= a->seq || r->seq >= b->seq)
            continue;
        if (!gate_stage_covers(r->src, a->stage))
            continue;
        if (!gate_stage_covers(r->dst, b->stage))
            continue;
        return r;
    }
    return NULL;
}

/* How many barriers of that recording were recorded strictly between the two writes, for the
 * checks that state how many lie there. */
static unsigned gate_barriers_between(const RecWrite *a, const RecWrite *b)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nbars; i++)
        if (c.bars_rec[i].rec == a->rec && c.bars_rec[i].seq > a->seq
            && c.bars_rec[i].seq < b->seq)
            n++;
    return n;
}

/* The submission-and-fence-wait boundary that orders the later recording after the earlier one,
 * or NULL. Two writes in different recordings are ordered only if the earlier recording was
 * ended, submitted, and its fence wait completed before the later command was recorded. */
static const RecSubmit *gate_ordering_submit(const RecWrite *a, const RecWrite *b)
{
    unsigned i;

    for (i = 0; i < c.nsubs; i++) {
        const RecSubmit *s = &c.subs[i];

        if (s->rec != a->rec || s->cb != a->cb)
            continue;
        if (!s->waited_seq || s->waited_seq >= b->seq)
            continue;
        return s;
    }
    return NULL;
}

/* The same boundary for an earlier READ and a later write: a fence wait that completed after the
 * read was recorded proves the read is over, which is exactly the execution order WAR needs. */
static const RecSubmit *gate_ordering_submit_read(const RecRead *a, const RecWrite *b)
{
    unsigned i;

    for (i = 0; i < c.nsubs; i++) {
        const RecSubmit *s = &c.subs[i];

        if (s->rec != a->rec || s->cb != a->cb)
            continue;
        if (!s->waited_seq || s->waited_seq >= b->seq)
            continue;
        return s;
    }
    return NULL;
}

static void range_of(const RecWrite *w, char *out, size_t n)
{
    if (!w) {
        snprintf(out, n, "(no such write)");
        return;
    }
    snprintf(out, n, "%s -> buffer %p [%llu, %llu) in command buffer %p (recording %u, slot %d)",
             w->how, (void *)w->buf, (unsigned long long)w->off,
             (unsigned long long)(w->off + w->size), (void *)w->cb, w->rec,
             w->slot == GATE_NOSLOT ? -1 : (int)w->slot);
}

static void read_range_of(const RecRead *r, char *out, size_t n)
{
    if (!r) {
        snprintf(out, n, "(no such read)");
        return;
    }
    snprintf(out, n, "%s -> buffer %p [%llu, %llu) in command buffer %p (recording %u, slot %d)",
             r->how, (void *)r->buf, (unsigned long long)r->off,
             (unsigned long long)(r->off + r->size), (void *)r->cb, r->rec,
             r->slot == GATE_NOSLOT ? -1 : (int)r->slot);
}

static const RecRead *gate_read_of(unsigned target, unsigned nth)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nreads; i++)
        if (c.reads[i].target == target)
            if (n++ == nth)
                return &c.reads[i];
    return NULL;
}

static unsigned gate_count_writes(const char *how, unsigned target)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nwrites; i++)
        if (c.writes[i].target == target
            && (how == NULL || strcmp(c.writes[i].how, how) == 0))
            n++;
    return n;
}

static const RecWrite *gate_write_of(const char *how, unsigned target, unsigned nth)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nwrites; i++)
        if (c.writes[i].target == target
            && (how == NULL || strcmp(c.writes[i].how, how) == 0))
            if (n++ == nth)
                return &c.writes[i];
    return NULL;
}

/* Decide every pair of overlapping accesses to the same buffer, and print the evidence: the whole
 * stream in recording order, then one line per overlapping pair with the barrier or the
 * submission-and-fence boundary that orders it, or the word UNORDERED. Two classes are decided:
 *
 *   write-after-write -- the later write needs a memory dependency, so the barrier between them
 *     must cover the earlier writer's stage AND access and the later writer's stage AND access;
 *   write-after-read (WAR) -- the later write must not start until the earlier read has finished,
 *     which is an EXECUTION dependency: the barrier between them must cover the reader's stage and
 *     the later writer's stage, and NO access mask is required for it.
 *
 * Cross-recording pairs use the submission-and-fence boundary instead, and that rule is
 * deliberately STRICTER than Vulkan: a submission whose fence wait completed certainly orders what
 * follows it, but Vulkan also allows other valid arrangements (a queue-family ownership transfer,
 * two submissions each with their own signal/wait pair, a semaphore wait) that this gate does not
 * model, so it decides THIS implementation's command stream and fails safe rather than accepting
 * every legal alternative.
 *
 * The counts are reported by gate_order_checks below, which is also where a scenario with no
 * recorded reuse says so. */
static void gate_decide(const char *tag, GateOrder *o)
{
    char ra[190], rb[190];
    unsigned i, j;

    memset(o, 0, sizeof *o);
    printf("\n      -- %s: the recorded stream --\n", tag);
    for (i = 0; i < c.nwrites; i++) {
        const RecWrite *w = &c.writes[i];

        range_of(w, ra, sizeof ra);
        printf("      #%u write %s\n", w->seq, ra);
    }
    for (i = 0; i < c.nreads; i++) {
        const RecRead *r = &c.reads[i];

        read_range_of(r, ra, sizeof ra);
        printf("      #%u read  %s (stage 0x%08X, access 0x%08X)\n", r->seq, ra, (unsigned)r->stage,
               (unsigned)r->access);
    }
    for (i = 0; i < c.nbars; i++) {
        const RecBar *r = &c.bars_rec[i];

        printf("      #%u vkCmdPipelineBarrier in recording %u: srcStage 0x%08X srcAccess 0x%08X"
               " -> dstStage 0x%08X dstAccess 0x%08X\n", r->seq, r->rec, (unsigned)r->src,
               (unsigned)r->srca, (unsigned)r->dst, (unsigned)r->dsta);
    }
    for (i = 0; i < c.nsubs; i++) {
        const RecSubmit *s = &c.subs[i];

        printf("      #%u vkQueueSubmit of recording %u (command buffer %p, slot %d)%s\n", s->seq,
               s->rec, (void *)s->cb, s->slot == GATE_NOSLOT ? -1 : (int)s->slot,
               s->waited_seq ? " -- its fence wait completed" : " -- never waited for");
    }
    if (c.nwaits)
        printf("      %u fence waits; slot 0: pending %d, npending %u, recording %d; "
               "g.st.submits %llu, drain_waits %llu\n", c.nwaits, g.slots[0].pending, g.npending,
               g.slots[0].recording, (unsigned long long)g.st.submits,
               (unsigned long long)g.st.drain_waits);

    for (i = 0; i < c.nwrites; i++) {
        for (j = i + 1u; j < c.nwrites; j++) {
            const RecWrite *a = &c.writes[i], *b = &c.writes[j];

            if (a->buf != b->buf || !gate_overlap(a, b))
                continue;
            o->reuses++;
            range_of(a, ra, sizeof ra);
            range_of(b, rb, sizeof rb);
            if (a->rec == b->rec) {
                const RecBar *r = gate_ordering_barrier(a, b);

                if (r)
                    printf("      ordered  #%u/%s and #%u/%s: the barrier at #%u (recording %u, "
                           "srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X dstAccess "
                           "0x%08X)\n", a->seq, a->how, b->seq, b->how, r->seq, r->rec,
                           (unsigned)r->src, (unsigned)r->srca, (unsigned)r->dst, (unsigned)r->dsta);
                else {
                    o->unordered++;
                    printf("      UNORDERED #%u/%s and #%u/%s in recording %u: no barrier "
                           "strictly between them orders the later write after the earlier one\n",
                           a->seq, a->how, b->seq, b->how, a->rec);
                }
            } else {
                const RecSubmit *s = gate_ordering_submit(a, b);

                if (s)
                    printf("      ordered  #%u/%s (recording %u) and #%u/%s (recording %u): the "
                           "submission at #%u and its fence wait at #%u\n", a->seq, a->how,
                           a->rec, b->seq, b->how, b->rec, s->seq, s->waited_seq);
                else {
                    o->unordered++;
                    printf("      UNORDERED #%u/%s (recording %u) and #%u/%s (recording %u): the "
                           "earlier recording was not submitted and waited for before the later "
                           "command was recorded\n", a->seq, a->how, a->rec, b->seq, b->how,
                           b->rec);
                }
            }
        }
    }
    /* The write-after-read class: every earlier read against every later write of the same
     * buffer. Ordered by an EXECUTION dependency (stages only), or by a completed fence wait for a
     * read in an earlier recording -- see this function's comment. */
    for (i = 0; i < c.nreads; i++) {
        const RecRead *a = &c.reads[i];

        for (j = 0; j < c.nwrites; j++) {
            const RecWrite *b = &c.writes[j];

            if (a->buf != b->buf || b->seq < a->seq)
                continue;
            if (!(a->off < b->off + b->size && b->off < a->off + a->size))
                continue;
            o->war_reuses++;
            read_range_of(a, ra, sizeof ra);
            range_of(b, rb, sizeof rb);
            if (a->rec == b->rec) {
                const RecBar *r = gate_ordering_barrier_war(a, b);

                if (r)
                    printf("      WAR ordered  read #%u and write #%u: the barrier at #%u "
                           "(recording %u, srcStage 0x%08X -> dstStage 0x%08X, stages only: an "
                           "execution dependency is what a write-after-read needs)\n", a->seq,
                           b->seq, r->seq, r->rec, (unsigned)r->src, (unsigned)r->dst);
                else {
                    o->war_unordered++;
                    printf("      WAR UNORDERED read #%u/%s and write #%u/%s in recording %u: no "
                           "barrier strictly between them has a source stage covering the reader "
                           "and a destination stage covering the later writer\n", a->seq, a->how,
                           b->seq, b->how, a->rec);
                }
            } else {
                const RecSubmit *s = gate_ordering_submit_read(a, b);

                if (s)
                    printf("      WAR ordered  read #%u (recording %u) and write #%u (recording "
                           "%u): the submission at #%u and its fence wait at #%u (the read is "
                           "over before the wait completes)\n", a->seq, a->rec, b->seq, b->rec,
                           s->seq, s->waited_seq);
                else {
                    o->war_unordered++;
                    printf("      WAR UNORDERED read #%u (recording %u) and write #%u (recording "
                           "%u): the earlier recording was not submitted and waited for before "
                           "the later command was recorded\n", a->seq, a->rec, b->seq, b->rec);
                }
            }
        }
    }

    g_reuses += o->reuses;
    g_unordered += o->unordered;
    g_war_reuses += o->war_reuses;
    g_war_unordered += o->war_unordered;
    printf("      overlapping reuses: %u, unordered: %u; WAR pairs: %u, war_unordered: %u\n",
           o->reuses, o->unordered, o->war_reuses, o->war_unordered);
    fflush(stdout);
}

/* The two checks the ordering decision ends in, phrased for whether the scenario MUST have
 * recorded a reuse. Property 4b's form records none -- its injected failure precedes every write
 * of the group -- and that is stated rather than papered over with a "0 of 0" that would read
 * like a reuse that was ordered. */
static void gate_order_checks(const char *tag, const GateOrder *o, int require_reuse)
{
    char what[160];

    if (require_reuse) {
        snprintf(what, sizeof what, "%s the overlap really happened: writes to the same buffer "
                                    "share bytes", tag);
        check(o->reuses > 0u, what, "%u overlapping pair(s) of recorded writes", o->reuses);
    } else {
        snprintf(what, sizeof what, "%s no reuse was recorded at all, and nothing is unordered",
                 tag);
        check(o->reuses == 0u && o->unordered == 0u, what,
              "overlapping reuses %u, unordered %u (this form's failure precedes every write of "
              "the group, so no abandoned write can be reused)", o->reuses, o->unordered);
    }
    if (require_reuse) {
        snprintf(what, sizeof what, "%s every overlapping reuse is ordered (unordered == 0)", tag);
        check(o->unordered == 0u, what, "unordered %u of %u overlapping pair(s)", o->unordered,
              o->reuses);
    }
    snprintf(what, sizeof what, "%s no overlapping WRITE-AFTER-READ pair is left unordered "
                                "(war_unordered == 0)", tag);
    check(o->war_unordered == 0u, what, "unordered %u of %u read/write pair(s) (a write-after-read "
          "needs an execution dependency: a barrier covering both stages)", o->war_unordered,
          o->war_reuses);
}

/* ── The shared property helpers ────────────────────────────────────────────────────── */

/* What a call must have published, as deltas of the counters nv2a_vk_backend_readback_stats
 * reports. */
typedef struct {
    uint64_t readbacks, batches, packed, packed_pixels, copied, bytes;
} RbWant;

static void want_nothing(RbWant *w)
{
    memset(w, 0, sizeof *w);
}

/* Every publication counter, checked one by one so a failure names the one that moved. */
static void check_counters(const char *tag, const Nv2aVkReadbackStats *b,
                           const Nv2aVkReadbackStats *a, const RbWant *w)
{
    char what[130];

    snprintf(what, sizeof what, "%s readbacks published (targets written to guest memory)", tag);
    check(a->readbacks == b->readbacks + w->readbacks, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->readbacks, (unsigned long long)a->readbacks,
          (unsigned long long)w->readbacks);
    snprintf(what, sizeof what, "%s batches published (submissions that reached a publish)", tag);
    check(a->batches == b->batches + w->batches, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->batches, (unsigned long long)a->batches,
          (unsigned long long)w->batches);
    snprintf(what, sizeof what, "%s packed 16-bit targets published", tag);
    check(a->packed == b->packed + w->packed, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->packed, (unsigned long long)a->packed,
          (unsigned long long)w->packed);
    snprintf(what, sizeof what, "%s packed pixels published", tag);
    check(a->packed_pixels == b->packed_pixels + w->packed_pixels, what,
          "%llu -> %llu (wanted +%llu)", (unsigned long long)b->packed_pixels,
          (unsigned long long)a->packed_pixels, (unsigned long long)w->packed_pixels);
    snprintf(what, sizeof what, "%s 4-byte targets copied out of their image", tag);
    check(a->copied == b->copied + w->copied, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->copied, (unsigned long long)a->copied,
          (unsigned long long)w->copied);
    snprintf(what, sizeof what, "%s bytes written into guest memory", tag);
    check(a->bytes == b->bytes + w->bytes, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->bytes, (unsigned long long)a->bytes,
          (unsigned long long)w->bytes);
}

/* The state a group that recorded nothing usable must leave the batch in. */
static void check_nothing_submitted(const char *tag, const RecWrite *a, const RecWrite *b)
{
    char what[130];
    unsigned i, subs_between = 0, waits_between = 0;

    for (i = 0; i < c.nsubs; i++)
        if (a && b && c.subs[i].seq > a->seq && c.subs[i].seq < b->seq)
            subs_between++;
    for (i = 0; i < c.nwaits; i++)
        if (a && b && c.wait_seq[i] > a->seq && c.wait_seq[i] < b->seq)
            waits_between++;

    snprintf(what, sizeof what, "%s nothing was submitted between the abandoned copies and the "
                                "next group", tag);
    check(subs_between == 0u && waits_between == 0u, what,
          "submissions between them %u, fence waits between them %u", subs_between, waits_between);
    snprintf(what, sizeof what, "%s the abandoned copies stayed in the recording slot 0 left "
                                "open for the next group", tag);
    check(a && b && a->cb == b->cb && a->rec == b->rec && b->slot == 0u && b->recording == 1
          && a->recording == 1, what,
          "both copies in command buffer %p (recording %u), slot 0 recording %d when the next "
          "group's copy was recorded", (void *)(b ? b->cb : VK_NULL_HANDLE), b ? b->rec : 0u,
          b ? b->recording : -1);
    snprintf(what, sizeof what, "%s the pass's next submission is the one that carried them", tag);
    check(c.submits == 1u && c.submitted[0] != VK_NULL_HANDLE && b && c.submitted[0] == b->cb
          && g.st.submits == 1u, what,
          "vkQueueSubmit %u, carrying command buffer %p (the abandoned copies' own: %p), "
          "g.st.submits %llu", c.submits, (void *)c.submitted[0],
          (void *)(b ? b->cb : VK_NULL_HANDLE), (unsigned long long)g.st.submits);
}

/* The injected outcome really was the one the case is about, and it really is not a device loss. */
static void check_injection(const char *tag, unsigned want_allocs)
{
    char what[130];

    snprintf(what, sizeof what, "%s the pack set allocation was attempted and really failed", tag);
    check(c.ds_allocs == want_allocs && c.ds_failures == 1u, what,
          "vkAllocateDescriptorSets %u, returning VK_ERROR_OUT_OF_POOL_MEMORY %u, pool %p",
          c.ds_allocs, c.ds_failures, (void *)c.ds_pool);
    snprintf(what, sizeof what, "%s the failure was reported (vb_fail), not latched", tag);
    check(g.fails == 1u && g.ok == 1 && g.failed == 0, what,
          "g.fails %u, g.ok %d, g.failed %d", g.fails, g.ok, g.failed);
}

/* The scenario's targets' guest bytes: each rectangle either holds the bytes the fixture
 * supplied for its own slice or is untouched. */
static void check_guest(const char *tag, const Scenario *s, unsigned published_mask)
{
    char what[130];
    unsigned k;

    for (k = 0; k < s->n; k++) {
        size_t bad;

        if (published_mask & (1u << k)) {
            bad = published_bad(s, k);
            snprintf(what, sizeof what, "%s %s's rectangle is exactly the bytes the fixture "
                                        "supplied for its own slice", tag, s->g[k].name);
            check(bad == 0u, what, "%zu of %llu bytes differ (slice at staging offset %llu)",
                  bad, (unsigned long long)s->g[k].slice, (unsigned long long)s->off[k]);
        } else {
            bad = marker_bad(s, k);
            snprintf(what, sizeof what, "%s %s's rectangle holds its pre-call bytes (never "
                                        "published)", tag, s->g[k].name);
            check(bad == 0u, what, "%zu of %llu bytes differ (checksum 0x%016llX)",
                  bad, (unsigned long long)s->g[k].slice,
                  (unsigned long long)hash_bytes(g_guest + s->g[k].va, (size_t)s->g[k].slice));
        }
    }
    snprintf(what, sizeof what, "%s no byte outside the rectangles was written", tag);
    check(tail_bad(s) == 0u, what, "the %zu-byte tail from guest offset 0x%X is %s", s->tail_len,
          (unsigned)s->tail_va, tail_bad(s) ? "CHANGED" : "unchanged");

    /* The published rectangle must also differ from the pattern of the slice the ABANDONED group
     * wrote first: that slice and the next group's target slice both start at staging offset 0, so
     * the comparison is what makes "its own bytes" a statement about the target and not about the
     * offset. It is made over the bytes the two slices can share, capped at one row. The next
     * group's target is the LAST one the scenario wires, and the abandoned group's first is the
     * first, in every scenario. */
    if (published_mask & (1u << (s->n - 1u))) {
        size_t n = (size_t)s->g[s->n - 1u].slice;
        size_t shared = (size_t)s->g[0].slice;

        if (shared > n)
            shared = n;
        if (shared > GATE_ROW)
            shared = GATE_ROW;
        fill_stage_slice(g_row, shared, s->g[0].stage);
        snprintf(what, sizeof what, "%s %s's rectangle is its own bytes, not the bytes the "
                                    "abandoned group's first slice starts with", tag,
                 s->g[s->n - 1u].name);
        check(memcmp(g_guest + s->g[s->n - 1u].va, g_row, shared) != 0, what,
              "%zu bytes compared at staging offset 0, where both groups' slices start "
              "(pattern 0x%02X vs 0x%02X)", shared, s->g[s->n - 1u].stage, s->g[0].stage);
    }
}

/* Which targets are still dirty, named: the flush's retry decision, and which target a pass
 * published. */
static void check_dirty(const char *tag, const Scenario *s, unsigned want)
{
    char what[130], list[200];
    unsigned k, dirty = 0;
    size_t at = 0;

    list[0] = '\0';
    for (k = 0; k < s->n; k++) {
        if (s->t[k]->dirty)
            dirty++;
        at += (size_t)snprintf(list + at, sizeof list - at, "%s%s %s", k ? ", " : "",
                               s->g[k].name, s->t[k]->dirty ? "dirty" : "clean");
    }
    snprintf(what, sizeof what, "%s exactly %u of the %u targets are still dirty", tag, want,
             s->n);
    check(dirty == want, what, "%s (%u of %u dirty)", list, dirty, s->n);
}

/* ── The failing pass, driven once and decided on both sides ───────────────────────── */

static void drive_failing_pass(Scenario *s, int small16, Nv2aVkReadbackStats *before,
                              Nv2aVkReadbackStats *after)
{
    scenario(s, small16);
    c.ds_fail_at = 1;                           /* the FIRST pack-set allocation: T1's */
    stats(before);
    vb_targets_flush(s->t, s->n);
    stats(after);
}

/* ── 0. the source under test, and the seam this case binds to ─────────────────────── */

static void prop0(void)
{
    Nv2aVkReadbackStats st;
    VbTarget probe;
    uint32_t stride;
    VkDeviceSize slice;
    char what[130];
    unsigned k;
    int chk = g_checks, bad = g_check_failures;

    section("0. the source under test, probed from the same bytes at build time");
    printf("      production bodies compiled from: %s\n", VB_RB_ORDER_GATE_SOURCE);
    printf("      sha256 %s\n", VB_RB_ORDER_GATE_SOURCE_SHA256);
    printf("      %u bytes\n", (unsigned)VB_RB_ORDER_GATE_SOURCE_BYTES);
    note("every function driven below is that file's own, compiled into this translation unit by "
         "#include; scripts/vk-readback-ordering-gate.py rejects the BUILD when a definition, a "
         "struct field or a global this fixture binds to is gone");
    marker("the batch-local success flag (int batch_ok = 1)", VB_RB_ORDER_GATE_HAS_BATCH_FLAG);
    marker("the whole-batch skip before the submit (if (!batch_ok))",
           VB_RB_ORDER_GATE_HAS_BATCH_SKIP);
    marker("an ordering command at the abandonment point (vb_barrier(); in that branch)",
           VB_RB_ORDER_GATE_ABANDON_BARRIER);
    marker("... and it is the broad one, not a narrowed barrier written for this site",
           VB_RB_ORDER_GATE_ABANDON_BARRIER_BROAD);
    marker("the publication counters incremented after that submit",
           VB_RB_ORDER_GATE_COUNTERS_AFTER_SUBMIT);

    snprintf(what, sizeof what, "the source records an ordering command where the group is "
                                "abandoned");
    check(VB_RB_ORDER_GATE_ABANDON_BARRIER && VB_RB_ORDER_GATE_ABANDON_BARRIER_BROAD, what,
          "abandonment barrier %d, broad %d", VB_RB_ORDER_GATE_ABANDON_BARRIER,
          VB_RB_ORDER_GATE_ABANDON_BARRIER_BROAD);
    check(VB_RB_ORDER_GATE_HAS_BATCH_FLAG && VB_RB_ORDER_GATE_HAS_BATCH_SKIP
          && VB_RB_ORDER_GATE_COUNTERS_AFTER_SUBMIT,
          "the batch's own failure path is the one this case injects into",
          "batch flag %d, batch skip %d, counters after the submit %d",
          VB_RB_ORDER_GATE_HAS_BATCH_FLAG, VB_RB_ORDER_GATE_HAS_BATCH_SKIP,
          VB_RB_ORDER_GATE_COUNTERS_AFTER_SUBMIT);

    check(VB_READBACK_MAX == GATE_CAP && VB_MAX_TARGETS >= 3u && VB_SLOTS == 3u,
          "the fixture's three targets and two groups fit the source's own bounds",
          "VB_READBACK_MAX %llu, VB_MAX_TARGETS %u, VB_SLOTS %u", (unsigned long long)VB_READBACK_MAX,
          (unsigned)VB_MAX_TARGETS, (unsigned)VB_SLOTS);
    check((uint64_t)T0_SLICE + (uint64_t)T1_SLICE == (uint64_t)GATE_CAP,
          "the first two targets fill the staging cap exactly, so the third starts a NEW group "
          "at slice offset 0 again",
          "T0 %llu + T1 %llu = %llu, cap %llu", (unsigned long long)T0_SLICE,
          (unsigned long long)T1_SLICE, (unsigned long long)(T0_SLICE + T1_SLICE),
          (unsigned long long)GATE_CAP);
    check(T0_SLICE <= GATE_CAP && T1_SLICE <= GATE_CAP && T2_SLICE <= GATE_CAP
          && T2P_SLICE <= GATE_CAP,
          "every single slice is inside the cap", "T0 %llu, T1 %llu, T2 %llu, T2' %llu, cap %llu",
          (unsigned long long)T0_SLICE, (unsigned long long)T1_SLICE,
          (unsigned long long)T2_SLICE, (unsigned long long)T2P_SLICE,
          (unsigned long long)GATE_CAP);
    check((uint64_t)TC_SLICE + (uint64_t)TD_SLICE < (uint64_t)GATE_CAP
          && (uint64_t)TC_SLICE + (uint64_t)TD_SLICE + (uint64_t)TE_SLICE > (uint64_t)GATE_CAP,
          "scenario B's first two targets fit the cap and its third one does not, so the third "
          "starts a NEW group at slice offset 0 again",
          "TC %llu + TD %llu = %llu, + TE %llu = %llu, cap %llu",
          (unsigned long long)TC_SLICE, (unsigned long long)TD_SLICE,
          (unsigned long long)(TC_SLICE + TD_SLICE), (unsigned long long)TE_SLICE,
          (unsigned long long)(TC_SLICE + TD_SLICE + TE_SLICE), (unsigned long long)GATE_CAP);
    check((size_t)(TE_VA + (uint32_t)TE_SLICE) == (size_t)(T2_VA + (uint32_t)T2_SLICE)
          && (size_t)(TE_VA + (uint32_t)TE_SLICE) <= GATE_GUEST
          && (size_t)(TD_VA + (uint32_t)TD_SLICE) == (size_t)TE_VA,
          "scenario B's three footprints are contiguous and end where scenario A's do, so one "
          "arena and one watched tail serve both",
          "TC+TD end 0x%X, TE end 0x%X, A's third target ends 0x%X, arena %llu",
          (unsigned)(TD_VA + (uint32_t)TD_SLICE), (unsigned)(TE_VA + (uint32_t)TE_SLICE),
          (unsigned)(T2_VA + (uint32_t)T2_SLICE), (unsigned long long)GATE_GUEST);

    /* The fixture's own geometry is the source's: the slice the production computes for each
     * target is the constant this case fills the staging buffer with, and each rectangle is the
     * whole guest surface, so a published rectangle is exactly its slice's bytes. */
    memset(&c, 0, sizeof c);
    for (k = 0; k < 7u; k++) {
        const GateTarget *d = &s_tdef[k];

        memset(&probe, 0, sizeof probe);
        wire_target(&probe, d, 0u);
        stride = 0;
        slice = vb_readback_slice(&probe, &stride);
        snprintf(what, sizeof what, "the fixture's slice for %s is the source's own answer",
                 d->name);
        check(slice == d->slice && stride == d->pitch
              && (VkDeviceSize)d->w * d->bpp == d->pitch, what,
              "vb_readback_slice %llu/%u, fixture %llu/%u (w*bpp %llu)",
              (unsigned long long)slice, stride, (unsigned long long)d->slice, d->pitch,
              (unsigned long long)((VkDeviceSize)d->w * d->bpp));
    }

    memset(&st, 0, sizeof st);
    nv2a_vk_backend_readback_stats(&st);
    check(st.readbacks == 0 && st.batches == 0 && st.packed == 0 && st.packed_pixels == 0
          && st.copied == 0 && st.bytes == 0 && st.incomplete == 0,
          "the published-work counters start at zero in a fresh world",
          "readbacks %llu, batches %llu, packed %llu, pixels %llu, copied %llu, bytes %llu, "
          "incomplete %llu", (unsigned long long)st.readbacks, (unsigned long long)st.batches,
          (unsigned long long)st.packed, (unsigned long long)st.packed_pixels,
          (unsigned long long)st.copied, (unsigned long long)st.bytes,
          (unsigned long long)st.incomplete);
    verdict("0 the source and its seam", chk, bad);
}

/* ── 1a. the failing pass, 4-byte third target ─────────────────────────────────────── */

static void prop1a(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    const RecWrite *w0, *w2;
    const RecBar *r;
    unsigned i, between = 0;
    char ra[190], rb[190], what[150];
    int chk = g_checks, bad = g_check_failures;

    section("1a. the failing pass, 4-byte third target: two copies write the same staging bytes");
    note("the injected outcome is VK_ERROR_OUT_OF_POOL_MEMORY from the stub, once, at T1's pack "
         "set -- after T0's vkCmdCopyImageToBuffer was recorded and before T2's");

    drive_failing_pass(&s, 0, &b, &a);
    check_injection("1a", 1u);

    w0 = gate_write_of(NULL, 0u, 0u);
    w2 = gate_write_of(NULL, 2u, 0u);

    snprintf(what, sizeof what, "1a the abandoned group had already recorded T0's copy into the "
                                "staging buffer");
    check(c.imgcopy == 2u && w0 && w0->buf == g.rb_buf && w0->off == (VkDeviceSize)0
          && w0->size == T0_SLICE, what,
          "vkCmdCopyImageToBuffer %u; T0's copy %p [%llu, %llu)", c.imgcopy,
          (void *)(w0 ? w0->buf : VK_NULL_HANDLE), (unsigned long long)(w0 ? w0->off : 0u),
          (unsigned long long)(w0 ? w0->off + w0->size : 0u));
    snprintf(what, sizeof what, "1a T1's dispatch was NOT recorded: the failure came before it");
    check(c.dispatches == 0u && c.bufcopy == 0u && c.ds_failures == 1u
          && c.ds_fail_seq > (w0 ? w0->seq : 0u), what,
          "pack dispatches %u, pack buffer copies %u, the failed set allocation at #%u after "
          "T0's copy at #%u", c.dispatches, c.bufcopy, c.ds_fail_seq, w0 ? w0->seq : 0u);

    snprintf(what, sizeof what, "1a the third target's copy writes the SAME bytes, in the SAME "
                                "command buffer");
    check(w2 && w2->buf == g.rb_buf && w2->off == (VkDeviceSize)0 && w2->size == T2_SLICE
          && w2->cb == (w0 ? w0->cb : VK_NULL_HANDLE) && w2->rec == (w0 ? w0->rec : 0u)
          && w2->cb == g.slots[0].cmd, what,
          "T2's copy %p [%llu, %llu) in command buffer %p (recording %u), T0's copy in %p "
          "(recording %u), slot 0's command buffer %p",
          (void *)(w2 ? w2->buf : VK_NULL_HANDLE), (unsigned long long)(w2 ? w2->off : 0u),
          (unsigned long long)(w2 ? w2->off + w2->size : 0u),
          (void *)(w2 ? w2->cb : VK_NULL_HANDLE), w2 ? w2->rec : 0u,
          (void *)(w0 ? w0->cb : VK_NULL_HANDLE), w0 ? w0->rec : 0u, (void *)g.slots[0].cmd);

    gate_decide("1a the failing pass", &o);
    gate_order_checks("1a the failing pass", &o, 1);

    /* The two ranges, quoted, and the count of the deciding reuse. */
    range_of(w0, ra, sizeof ra);
    range_of(w2, rb, sizeof rb);
    snprintf(what, sizeof what, "1a the deciding reuse is exactly the two copies of the same "
                                "staging bytes");
    check(o.reuses == 1u, what, "reuses %u; T0: %s; T2: %s", o.reuses, ra, rb);

    for (i = 0; i < c.nbars; i++)
        if (w0 && w2 && c.bars_rec[i].seq > w0->seq && c.bars_rec[i].seq < w2->seq
            && c.bars_rec[i].rec == w0->rec)
            between++;
    r = (w0 && w2) ? gate_ordering_barrier(w0, w2) : NULL;
    snprintf(what, sizeof what, "1a exactly one barrier lies strictly between the two writes");
    check(between == 1u, what, "barriers between them %u", between);
    snprintf(what, sizeof what, "1a that barrier is the broad abandonment barrier the repair "
                                "records before it steps over the group");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what,
          "%s (wanted srcStage ALL_COMMANDS 0x%08X srcAccess MEMORY_WRITE 0x%08X -> dstStage "
          "ALL_COMMANDS 0x%08X dstAccess READ|WRITE 0x%08X)",
          r ? "srcStage/access and dstStage/access as required" : "NO ordering barrier between "
          "them at all", (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)VK_ACCESS_MEMORY_WRITE_BIT, (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));
    if (r)
        note("the barrier that orders them: recording %u, srcStage 0x%08X srcAccess 0x%08X -> "
             "dstStage 0x%08X dstAccess 0x%08X (buffer %p, ranges [0, %llu) and [0, %llu))",
             r->rec, (unsigned)r->src, (unsigned)r->srca, (unsigned)r->dst, (unsigned)r->dsta,
             (void *)g.rb_buf, (unsigned long long)T0_SLICE, (unsigned long long)T2_SLICE);

    verdict("1a the failing pass: the reuse is real and ordered", chk, bad);
}

/* ── 1b. the failing pass, 16-bit third target: the pack buffers ───────────────────── */

static void prop1b(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    const RecWrite *w0, *wd, *w2;
    const RecBar *r;
    unsigned rx = (T2P_W + 15u) / 16u, ry = (T2P_H + 7u) / 8u;
    char ra[190], rb[190], what[150];
    int chk = g_checks, bad = g_check_failures;

    section("1b. the failing pass, 16-bit third target: the pack buffer's own reuse");
    note("the third target is 16-bit here, so the group after the abandonment records a pack "
         "dispatch (a shader write into g.pack_buf) and the buffer copy that reads it (a write "
         "into g.rb_buf), both after the abandoned T0 copy");

    drive_failing_pass(&s, 1, &b, &a);
    check_injection("1b", 2u);

    w0 = gate_write_of(NULL, 0u, 0u);
    wd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 2u, 0u);
    w2 = gate_write_of("vkCmdCopyBuffer", 2u, 0u);

    snprintf(what, sizeof what, "1b the pack dispatch's write into the pack buffer is the push "
                                "block's own range");
    check(c.dispatches == 1u && c.pushes == 1u && c.push.rect[0] == 0u && c.push.rect[1] == 0u
          && c.push.rect[2] == T2P_W && c.push.rect[3] == T2P_H && c.push.dst[0] == 0u
          && c.push.dst[1] == T2P_PITCH && c.push.dst[2] == 0u && c.push.dst[3] == 0u
          && c.dispatch_group[0] == rx && c.dispatch_group[1] == ry && c.dispatch_group[2] == 1u
          && wd && wd->buf == g.pack_buf && wd->off == (VkDeviceSize)0
          && wd->size == T2P_SLICE, what,
          "dispatch %u (%ux%ux%u), push rect {%u,%u,%u,%u} dst {%u,%u,%u,%u}, write %p "
          "[%llu, %llu)", c.dispatches, c.dispatch_group[0], c.dispatch_group[1],
          c.dispatch_group[2], c.push.rect[0], c.push.rect[1], c.push.rect[2], c.push.rect[3],
          c.push.dst[0], c.push.dst[1], c.push.dst[2], c.push.dst[3],
          (void *)(wd ? wd->buf : VK_NULL_HANDLE), (unsigned long long)(wd ? wd->off : 0u),
          (unsigned long long)(wd ? wd->off + wd->size : 0u));
    snprintf(what, sizeof what, "1b the packed slice was copied into the staging buffer at the "
                                "slice offset the abandoned copy used");
    check(c.bufcopy == 1u && w2 && w2->buf == g.rb_buf && w2->off == (VkDeviceSize)0
          && w2->size == T2P_SLICE && c.bufcopy_src == g.pack_buf
          && c.bufcopy_dst == g.rb_buf && c.bufcopy_region.srcOffset == (VkDeviceSize)0
          && c.bufcopy_region.dstOffset == (VkDeviceSize)0, what,
          "vkCmdCopyBuffer %u: %p -> %p [%llu, %llu)", c.bufcopy, (void *)c.bufcopy_src,
          (void *)c.bufcopy_dst, (unsigned long long)(w2 ? w2->off : 0u),
          (unsigned long long)(w2 ? w2->off + w2->size : 0u));

    gate_decide("1b the failing pass, pack variant", &o);
    gate_order_checks("1b the failing pass, pack variant", &o, 1);

    range_of(w0, ra, sizeof ra);
    range_of(w2, rb, sizeof rb);
    snprintf(what, sizeof what, "1b the deciding reuse is the abandoned copy and the buffer copy "
                                "of the packed slice");
    check(o.reuses == 1u && w0 && w2 && w2->cb == w0->cb && o.unordered == 0u, what,
          "reuses %u, unordered %u; T0: %s; third target: %s", o.reuses, o.unordered, ra, rb);

    /* Which barrier orders it, quoted: the abandoned group's own copy into the staging buffer and
     * the buffer copy that reuses those bytes with the next group's pack output. The pack path's
     * own barrier -- the one between ITS dispatch and the buffer copy that reads that slice -- is
     * deliberately narrower than this pair: COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ
     * orders that dispatch's write before the copy's READ of that slice and names no destination
     * write, so it cannot order this reuse. The barrier that has to cover it is the broad
     * abandonment barrier the repair records at the !batch_ok branch, and this variant therefore
     * decides the repair exactly as 1a does. */
    r = (w0 && w2) ? gate_ordering_barrier(w0, w2) : NULL;
    snprintf(what, sizeof what, "1b the broad abandonment barrier orders the abandoned copy's "
                                "write against the pack-output copy that reuses those bytes");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what,
          "%s (wanted srcStage ALL_COMMANDS 0x%08X srcAccess MEMORY_WRITE 0x%08X -> dstStage "
          "ALL_COMMANDS 0x%08X dstAccess READ|WRITE 0x%08X)",
          r ? "srcStage/access and dstStage/access as required" : "NO ordering barrier between "
          "them at all", (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)VK_ACCESS_MEMORY_WRITE_BIT, (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));
    if (r)
        note("the barrier that orders them: recording %u, srcStage 0x%08X srcAccess 0x%08X -> "
             "dstStage 0x%08X dstAccess 0x%08X (buffer %p, ranges [0, %llu) and [0, %llu))",
             r->rec, (unsigned)r->src, (unsigned)r->srca, (unsigned)r->dst, (unsigned)r->dsta,
             (void *)g.rb_buf, (unsigned long long)T0_SLICE, (unsigned long long)T2P_SLICE);
    note("barriers recorded between the abandoned copy and the packed slice's buffer copy: %u; the "
         "pack path's own (COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ) is one of them, "
         "but it orders the dispatch's write before the copy's READ of that slice and does not "
         "name that copy's write into g.rb_buf, so the reuse is ordered by the abandonment barrier "
         "-- which the pre-fix source records nowhere, so 1b is RED there as well",
         (w0 && w2) ? gate_barriers_between(w0, w2) : 0u);

    verdict("1b the failing pass, 16-bit third target (pack buffers)", chk, bad);
}

/* ── 1c. the failing pass, scenario B: the pack-path write class ───────────────────── */

/* Scenario B's failing pass: TC's pack set allocates, TD's is the injected failure, so the group
 * is abandoned after TC's dispatch, its pack barrier and its buffer copy into g.rb_buf. */
static void drive_failing_pass_b(Scenario *s, Nv2aVkReadbackStats *before,
                                 Nv2aVkReadbackStats *after)
{
    scenario_b(s);
    c.ds_fail_at = 2;                           /* the SECOND pack-set allocation: TD's */
    stats(before);
    vb_targets_flush(s->t, s->n);
    stats(after);
}

static void prop1c(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecWrite *wd, *wc, *we, *last;
    const RecBar *r;
    unsigned i, rx = (TC_W + 15u) / 16u, ry = (TC_H + 7u) / 8u;
    char ra[190], rb[190], what[160];
    int chk = g_checks, bad = g_check_failures;

    section("1c. the failing pass, scenario B: the abandoned group's LAST write is a pack-output "
            "buffer copy");
    note("TC's pack set allocates (allocation 1) and TD's is the injected failure (allocation 2), "
         "so the group is abandoned AFTER TC's dispatch, TC's pack barrier and TC's "
         "vkCmdCopyBuffer into g.rb_buf [0, %llu); TE does not fit and starts the next group at "
         "slice offset 0 again", (unsigned long long)TC_SLICE);

    drive_failing_pass_b(&s, &b, &a);
    check_injection("1c", 2u);

    wd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 0u, 0u);
    wc = gate_write_of("vkCmdCopyBuffer", 0u, 0u);
    we = gate_write_of(NULL, 2u, 0u);

    snprintf(what, sizeof what, "1c the abandoned group had recorded TC's pack dispatch and its "
                                "buffer copy into the staging buffer");
    check(c.dispatches == 1u && c.bufcopy == 1u && wd && wd->buf == g.pack_buf
          && wd->off == (VkDeviceSize)0 && wd->size == TC_SLICE && wc && wc->buf == g.rb_buf
          && wc->off == (VkDeviceSize)0 && wc->size == TC_SLICE, what,
          "dispatches %u, buffer copies %u; the dispatch writes %p [%llu, %llu), the copy writes "
          "%p [%llu, %llu)", c.dispatches, c.bufcopy,
          (void *)(wd ? wd->buf : VK_NULL_HANDLE), (unsigned long long)(wd ? wd->off : 0u),
          (unsigned long long)(wd ? wd->off + wd->size : 0u),
          (void *)(wc ? wc->buf : VK_NULL_HANDLE), (unsigned long long)(wc ? wc->off : 0u),
          (unsigned long long)(wc ? wc->off + wc->size : 0u));
    snprintf(what, sizeof what, "1c TC's packed slice is its own rectangle at slice offset 0, with "
                                "its own row stride");
    check(c.pushes == 1u && c.push.rect[0] == 0u && c.push.rect[1] == 0u
          && c.push.rect[2] == TC_W && c.push.rect[3] == TC_H && c.push.dst[0] == 0u
          && c.push.dst[1] == TC_PITCH && c.dispatch_group[0] == rx && c.dispatch_group[1] == ry
          && c.dispatch_group[2] == 1u, what,
          "push rect {%u,%u,%u,%u} dst {%u,%u,%u,%u}, groups %ux%ux%u", c.push.rect[0],
          c.push.rect[1], c.push.rect[2], c.push.rect[3], c.push.dst[0], c.push.dst[1],
          c.push.dst[2], c.push.dst[3], c.dispatch_group[0], c.dispatch_group[1],
          c.dispatch_group[2]);

    last = NULL;
    for (i = 0; i < c.nwrites; i++)
        if (c.writes[i].seq < c.ds_fail_seq && (!last || c.writes[i].seq > last->seq))
            last = &c.writes[i];
    snprintf(what, sizeof what, "1c that buffer copy is the abandoned recording's LAST write: the "
                                "failure came after it, nothing between them");
    check(last == wc && c.ds_fail_seq > (wc ? wc->seq : 0u), what,
          "the write before the failed allocation at #%u is #%u/%s", c.ds_fail_seq,
          last ? last->seq : 0u, last ? last->how : "(none)");

    /* The abandoned dispatch's pack-buffer write against what follows the abandonment. The pack
     * path's own barrier -- between this dispatch and the buffer copy that reads its slice -- is
     * now COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ: it orders the dispatch's write
     * before that copy's READ of the slice, names no destination write, and so cannot order the
     * reuse of pack-buffer bytes ACROSS a group boundary. What orders that class is the broad
     * abandonment barrier the repair records before it steps over the group -- which is why the
     * abandoned group's device writes are never the vulnerable ones. The vulnerable one is the
     * copy into g.rb_buf below, decided on the reuse itself. */
    r = (wd && we) ? gate_ordering_barrier(wd, we) : NULL;
    snprintf(what, sizeof what, "1c the abandoned dispatch's pack-buffer write is separated from "
                                "the next group's staging write by the broad abandonment barrier");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what,
          "%s (wanted srcStage ALL_COMMANDS 0x%08X srcAccess MEMORY_WRITE 0x%08X -> dstStage "
          "ALL_COMMANDS 0x%08X dstAccess READ|WRITE 0x%08X)",
          r ? "srcStage/access and dstStage/access as required" : "NO ordering barrier between "
          "them at all", (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)VK_ACCESS_MEMORY_WRITE_BIT, (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));

    check_nothing_submitted("1c", wc, we);
    snprintf(what, sizeof what, "1c the whole pass recorded into ONE command buffer: the abandoned "
                                "pack path's last command and TE's copy share it");
    check(c.begins == 1u && wc && we && we->cb == wc->cb && we->rec == wc->rec, what,
          "recordings begun %u, both writes in command buffer %p (recording %u)", c.begins,
          (void *)(we ? we->cb : VK_NULL_HANDLE), we ? we->rec : 0u);

    gate_decide("1c the failing pass, scenario B", &o);
    gate_order_checks("1c the failing pass, scenario B", &o, 1);

    range_of(wc, ra, sizeof ra);
    range_of(we, rb, sizeof rb);
    snprintf(what, sizeof what, "1c the deciding reuse is the abandoned buffer copy and TE's own "
                                "copy of the same staging bytes");
    check(o.reuses == 1u && o.unordered == 0u, what, "reuses %u, unordered %u; abandoned: %s; TE: "
          "%s", o.reuses, o.unordered, ra, rb);
    snprintf(what, sizeof what, "1c exactly one barrier lies strictly between those two writes");
    check(gate_barriers_between(wc, we) == 1u, what, "barriers between them %u",
          gate_barriers_between(wc, we));
    r = (wc && we) ? gate_ordering_barrier(wc, we) : NULL;
    snprintf(what, sizeof what, "1c that barrier is the broad abandonment barrier: the same one 1a "
                                "decides, ordering a buffer copy this time");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what, "%s",
          r ? "srcStage/access and dstStage/access as required"
            : "NO ordering barrier between them at all");

    check_dirty("1c", &s, 2u);
    snprintf(what, sizeof what, "1c TE was published and TC and TD are the two left dirty");
    check(s.t[2]->dirty == 0 && s.t[0]->dirty == 1 && s.t[1]->dirty == 1, what,
          "TC dirty %d, TD dirty %d, TE dirty %d", s.t[0]->dirty, s.t[1]->dirty, s.t[2]->dirty);
    check_guest("1c", &s, 0x4u);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.copied = 1u;                              /* TE: 4-byte, straight out of its image */
    w.bytes = (uint64_t)TE_SLICE;
    check_counters("1c", &b, &a, &w);
    snprintf(what, sizeof what, "1c the injected failure is not a device loss and left nothing "
                                "unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0
          && g.fails == 1u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d, g.fails %u",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed, g.fails);

    verdict("1c the failing pass, the pack-path write class", chk, bad);
}

/* ── 1d. the failing pass, scenario C: the pack buffer's WAR class ─────────────────── */

/* Scenario C's failing pass: TC2's pack set allocates and TX holds the group's second slice, so
 * the group is abandoned -- at TD2's set -- with TC2's dispatch, TC2's pack barrier and TC2's
 * buffer copy (which READS g.pack_buf [0, 32 MiB)) already recorded. TF starts the next group at
 * slice offset 0 and its own dispatch WRITES g.pack_buf [0, 32 MiB) again. */
static void drive_failing_pass_c(Scenario *s, Nv2aVkReadbackStats *before,
                                 Nv2aVkReadbackStats *after)
{
    scenario_c(s);
    c.ds_fail_at = 2;                           /* the SECOND pack-set allocation: TD2's */
    stats(before);
    vb_targets_flush(s->t, s->n);
    stats(after);
}

static void prop1d(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecRead *rd;
    const RecWrite *wd, *wc, *tw, *fd, *fw;
    const RecBar *r;
    char ra[190], rb[190], what[170];
    int chk = g_checks, bad = g_check_failures;

    section("1d. the failing pass, scenario C: the abandoned pack buffer read, and the next "
            "group's write of the same bytes");
    note("TC2 (16-bit, the group's FIRST staged target, slice at offset 0) allocates its pack set "
         "and records its dispatch, its pack barrier and its buffer copy, which reads g.pack_buf "
         "[0, %llu); TD2's set is the injected failure and abandons the group; TF then starts the "
         "next group at slice offset 0 and its own dispatch writes g.pack_buf [0, %llu) -- the "
         "write-after-read pair a log of writes alone cannot see",
         (unsigned long long)TC2_SLICE, (unsigned long long)TC2_SLICE);

    drive_failing_pass_c(&s, &b, &a);
    check_injection("1d", 3u);

    rd = gate_read_of(0u, 0u);
    wd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 0u, 0u);
    wc = gate_write_of("vkCmdCopyBuffer", 0u, 0u);
    tw = gate_write_of(NULL, 1u, 0u);
    fd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 3u, 0u);
    fw = gate_write_of("vkCmdCopyBuffer", 3u, 0u);

    snprintf(what, sizeof what, "1d the abandoned recording's read really is the pack buffer's "
                                "[0, %llu), and it is a TRANSFER read",
             (unsigned long long)TC2_SLICE);
    check(rd && rd->buf == g.pack_buf && rd->off == (VkDeviceSize)0 && rd->size == TC2_SLICE
          && rd->stage == VK_PIPELINE_STAGE_TRANSFER_BIT
          && rd->access == VK_ACCESS_TRANSFER_READ_BIT, what,
          "%s -> buffer %p [%llu, %llu) (stage 0x%08X, access 0x%08X)",
          rd ? rd->how : "(no read recorded)", (void *)(rd ? rd->buf : VK_NULL_HANDLE),
          (unsigned long long)(rd ? rd->off : 0u),
          (unsigned long long)(rd ? rd->off + rd->size : 0u), rd ? (unsigned)rd->stage : 0u,
          rd ? (unsigned)rd->access : 0u);
    snprintf(what, sizeof what, "1d the abandoned group had recorded that pack path in full: its "
                                "dispatch, and the buffer copy that reads and writes it");
    check(wd && wd->buf == g.pack_buf && wd->off == (VkDeviceSize)0 && wd->size == TC2_SLICE
          && wc && wc->buf == g.rb_buf && wc->off == (VkDeviceSize)0 && wc->size == TC2_SLICE
          && rd && rd->seq > wd->seq && wc->seq > rd->seq, what,
          "dispatch #%u writes %p [0, %llu), read #%u reads %p [0, %llu), copy #%u writes %p "
          "[0, %llu)", wd ? wd->seq : 0u, (void *)g.pack_buf, (unsigned long long)TC2_SLICE,
          rd ? rd->seq : 0u, (void *)g.pack_buf, (unsigned long long)TC2_SLICE, wc ? wc->seq : 0u,
          (void *)g.rb_buf, (unsigned long long)TC2_SLICE);
    snprintf(what, sizeof what, "1d the next group's dispatch writes exactly the bytes the "
                                "abandoned read read");
    check(fd && fd->buf == g.pack_buf && fd->off == (VkDeviceSize)0 && fd->size == TC2_SLICE
          && fw && fw->buf == g.rb_buf && fw->off == (VkDeviceSize)0 && fw->size == TC2_SLICE, what,
          "TF's dispatch #%u writes %p [%llu, %llu); the abandoned read was %p [%llu, %llu)",
          fd ? fd->seq : 0u, (void *)(fd ? fd->buf : VK_NULL_HANDLE),
          (unsigned long long)(fd ? fd->off : 0u),
          (unsigned long long)(fd ? fd->off + fd->size : 0u),
          (void *)(rd ? rd->buf : VK_NULL_HANDLE), (unsigned long long)(rd ? rd->off : 0u),
          (unsigned long long)(rd ? rd->off + rd->size : 0u));

    gate_decide("1d the failing pass, scenario C", &o);
    gate_order_checks("1d the failing pass, scenario C", &o, 1);

    read_range_of(rd, ra, sizeof ra);
    range_of(fd, rb, sizeof rb);
    snprintf(what, sizeof what, "1d the write-after-read pair is decided, not merely recorded");
    check(o.war_reuses == 1u && o.war_unordered == 0u, what,
          "WAR pairs %u, war_unordered %u; read: %s; later write: %s", o.war_reuses,
          o.war_unordered, ra, rb);
    r = (rd && fd) ? gate_ordering_barrier_war(rd, fd) : NULL;
    snprintf(what, sizeof what, "1d the barrier that orders the write after the read covers the "
                                "reader's stage and the writer's stage (no access mask: a "
                                "write-after-read is an execution dependency)");
    check(r && gate_stage_covers(r->src, VK_PIPELINE_STAGE_TRANSFER_BIT)
          && gate_stage_covers(r->dst, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT), what,
          "%s (srcStage 0x%08X -> dstStage 0x%08X, access masks 0x%08X -> 0x%08X, deliberately not "
          "consulted)", r ? "the abandonment barrier" : "NO barrier between them",
          r ? (unsigned)r->src : 0u, r ? (unsigned)r->dst : 0u, r ? (unsigned)r->srca : 0u,
          r ? (unsigned)r->dsta : 0u);
    note("the deciding PARITY's other half is the write-after-write class on g.rb_buf: TC2's "
         "abandoned copy writes [0, %llu) and TF's own copy writes it again, ordered by the same "
         "abandonment barrier (unordered %u of %u write/write pairs)",
         (unsigned long long)TC2_SLICE, o.unordered, o.reuses);

    /* The two dispatches' own reuse of the pack buffer, across the abandoned group: the abandoned
     * dispatch's write is ordered after the next group's by the broad abandonment barrier, not by
     * the pack path's own one -- that one is COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ
     * and orders a single dispatch's write before the copy that reads that slice, which is why the
     * class the writes alone cannot show needs the abandonment barrier. */
    r = (wd && fd) ? gate_ordering_barrier(wd, fd) : NULL;
    snprintf(what, sizeof what, "1d the two dispatches write the same pack buffer bytes, and the "
                                "broad abandonment barrier orders the later one after the "
                                "abandoned one");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what,
          "%s (wanted srcStage ALL_COMMANDS 0x%08X srcAccess MEMORY_WRITE 0x%08X -> dstStage "
          "ALL_COMMANDS 0x%08X dstAccess READ|WRITE 0x%08X)",
          r ? "srcStage/access and dstStage/access as required" : "NO ordering barrier between "
          "them at all", (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)VK_ACCESS_MEMORY_WRITE_BIT, (unsigned)VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
          (unsigned)(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT));

    check_nothing_submitted("1d", tw, fd);
    check_dirty("1d", &s, 3u);
    snprintf(what, sizeof what, "1d TF was published and TX, TC2 and TD2 are the three left dirty");
    check(s.t[3]->dirty == 0 && s.t[0]->dirty == 1 && s.t[1]->dirty == 1 && s.t[2]->dirty == 1,
          what, "TC2 dirty %d, TX dirty %d, TD2 dirty %d, TF dirty %d", s.t[0]->dirty,
          s.t[1]->dirty, s.t[2]->dirty, s.t[3]->dirty);
    check_guest("1d", &s, 0x8u);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.packed = 1u;                              /* TF is 16-bit */
    w.packed_pixels = (uint64_t)TF_W * TF_H;
    w.bytes = (uint64_t)TF_SLICE;
    check_counters("1d", &b, &a, &w);
    snprintf(what, sizeof what, "1d the injected failure is not a device loss and left nothing "
                                "unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0
          && g.fails == 1u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d, g.fails %u",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed, g.fails);

    verdict("1d the failing pass, the pack buffer's WAR class", chk, bad);
}

/* ── 2. the failing pass as a state ────────────────────────────────────────────────── */

static void prop2(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    RbWant w;
    const RecWrite *w0, *w2;
    char what[150];
    int chk = g_checks, bad = g_check_failures;

    section("2. the failing pass: the third target's own rectangle, and nothing else");
    note("the same pass as 1a, decided now on what it published and what it left behind");

    scenario(&s, 0);
    c.ds_fail_at = 1;
    stats(&b);
    vb_targets_flush(s.t, s.n);
    stats(&a);

    w0 = gate_write_of(NULL, 0u, 0u);
    w2 = gate_write_of(NULL, 2u, 0u);

    check_injection("2", 1u);
    check_dirty("2", &s, 2u);
    snprintf(what, sizeof what, "2 the third target was published: it is the clean one");
    check(s.t[2]->dirty == 0, what, "T0 dirty %d, T1 dirty %d, %s dirty %d", s.t[0]->dirty,
          s.t[1]->dirty, s.g[2].name, s.t[2]->dirty);
    check_nothing_submitted("2", w0, w2);

    /* It published exactly once, from the bytes the fixture supplied for its OWN slice. */
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.copied = 1u;                                  /* a 4-byte target: straight out of its image */
    w.bytes = (uint64_t)T2_SLICE;
    check_counters("2", &b, &a, &w);
    check_guest("2", &s, 0x4u);
    snprintf(what, sizeof what, "2 the third target's own slice is where its bytes came from");
    check(w2 && w2->off == (VkDeviceSize)0 && w2->size == T2_SLICE
          && w2->cb == g.slots[0].cmd, what,
          "its copy writes [%llu, %llu) of %p, which is exactly its slice at staging offset 0 "
          "(the abandoned copy's slice starts at 0 too)",
          (unsigned long long)(w2 ? w2->off : 0u),
          (unsigned long long)(w2 ? w2->off + w2->size : 0u),
          (void *)(w2 ? w2->buf : VK_NULL_HANDLE));

    snprintf(what, sizeof what, "2 the fixture supplied the bytes of exactly the batch that was "
                                "submitted, for the third target's own slice");
    check(c.supply_calls == 1u && c.supply_writes == 2u && c.supply_target == 2u
          && c.supply_off == (VkDeviceSize)0 && c.supply_size == T2_SLICE, what,
          "%u supply call(s) for %u write(s); the last supplied write is [%llu, %llu) of the "
          "third target's slice -- the abandoned copy was supplied in the same batch, because it "
          "rode in that submission", c.supply_calls, c.supply_writes,
          (unsigned long long)c.supply_off, (unsigned long long)(c.supply_off + c.supply_size));

    snprintf(what, sizeof what, "2 the injected failure is not a device loss and left nothing "
                                "unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0
          && g.fails == 1u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d, g.fails %u",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed, g.fails);

    note("the design's clause 'submitted nothing, waited for nothing, left slot 0 recording' is "
         "asserted OF THE ABANDONED BATCH above. This pass's whole call does submit once (and "
         "wait once), for the third target's own group: c.submits %u, c.waits %u, c.invalidates "
         "%u, c.begins %u -- the abandoned copies ride in that submission, which is the hazard "
         "1a decides", c.submits, c.waits, c.invalidates, c.begins);

    verdict("2 the failing pass: the third target's own rectangle, nothing else",
            chk, bad);
}

/* ── 3. the recovery control through the boundary ──────────────────────────────────── */

static void prop3(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecWrite *w0;
    int rc;
    char what[150];
    int chk = g_checks, bad = g_check_failures;

    section("3. the recovery control: the boundary publishes all three, with one retry");
    note("driven through vb_targets_flush_recover -- the entry point the flips, the target "
         "release and the guest-texture paths drive -- with the same one-shot injection; the "
         "boundary's own vb_readback_recover submits and drains the abandoned recording, then "
         "retries exactly once");

    scenario(&s, 0);
    c.ds_fail_at = 1;
    stats(&b);
    rc = vb_targets_flush_recover(s.t, s.n);
    stats(&a);

    snprintf(what, sizeof what, "3 the boundary published everything: its own return value says so");
    check(rc == 1, what, "vb_targets_flush_recover returned %d (0 would mean a target stayed "
                         "dirty)", rc);
    check_dirty("3", &s, 0u);
    check_guest("3", &s, 0x7u);
    want_nothing(&w);
    w.readbacks = 3u;
    w.batches = 2u;
    w.packed = 1u;
    w.packed_pixels = (uint64_t)T1_W * T1_H;
    w.copied = 2u;                                  /* T0 and the 4-byte third target */
    w.bytes = (uint64_t)T0_SLICE + (uint64_t)T1_SLICE + (uint64_t)T2_SLICE;
    check_counters("3", &b, &a, &w);

    /* Exactly one retry, and the ring was free before it recorded. */
    w0 = gate_write_of(NULL, 0u, 0u);
    snprintf(what, sizeof what, "3 exactly one retry: the flush was entered twice, and T0's copy "
                                "is recorded once per attempt");
    check(c.begins == 2u && c.nbegins == 2u && gate_count_writes("vkCmdCopyImageToBuffer", 0u) == 2u
          && c.ds_allocs == 2u && c.ds_failures == 1u && w0 != NULL, what,
          "recordings begun %u, T0 copies recorded %u, pack-set attempts %u (failures %u)",
          c.begins, gate_count_writes("vkCmdCopyImageToBuffer", 0u), c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "3 the slot was free again (pending 0, npending 0, fence and pool "
                                "reset) before the retry recorded");
    check(c.nbegins == 2u && c.beg[1].rec == 2u && c.beg[1].npending == 0u
          && c.beg[1].pending0 == 0 && c.beg[1].recording0 == 0
          && c.beg[1].fence_resets >= 1u && c.beg[1].pool_resets >= 1u, what,
          "at the retry's recording #%u: npending %u, slot 0 pending %d recording %d, fence "
          "resets %u, pool resets %u", c.nbegins == 2u ? c.beg[1].rec : 0u,
          c.nbegins == 2u ? c.beg[1].npending : 0u, c.nbegins == 2u ? c.beg[1].pending0 : -1,
          c.nbegins == 2u ? c.beg[1].recording0 : -1, c.nbegins == 2u ? c.beg[1].fence_resets : 0u,
          c.nbegins == 2u ? c.beg[1].pool_resets : 0u);

    gate_decide("3 both passes", &o);
    gate_order_checks("3 both passes", &o, 1);
    snprintf(what, sizeof what, "3 across both passes the reuse is real and every one is ordered");
    check(o.reuses == 3u && o.unordered == 0u, what,
          "overlapping reuses %u, unordered %u (the failing pass's copy of the third target over "
          "the abandoned T0 copy, and the retry's T0 copy over both of them across the submission "
          "and fence wait the boundary performed)", o.reuses, o.unordered);

    snprintf(what, sizeof what, "3 the boundary counted nothing as left unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed);

    verdict("3 the recovery control through the boundary", chk, bad);
}

/* ── 3b. the recovery control, scenario B ──────────────────────────────────────────── */

static void prop3b(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecWrite *wc;
    int rc;
    char what[160];
    int chk = g_checks, bad = g_check_failures;

    section("3b. the recovery control, scenario B: TC and TD publish on the retry");
    note("the same boundary as property 3 -- vb_targets_flush_recover -- on scenario B, whose "
         "abandoned group recorded a 16-bit target's pack dispatch and its buffer copy");

    scenario_b(&s);
    c.ds_fail_at = 2;                           /* TD's set: TC's whole pack path is recorded */
    stats(&b);
    rc = vb_targets_flush_recover(s.t, s.n);
    stats(&a);

    snprintf(what, sizeof what, "3b the boundary published everything: its own return value says so");
    check(rc == 1, what, "vb_targets_flush_recover returned %d (0 would mean a target stayed "
                         "dirty)", rc);
    check_dirty("3b", &s, 0u);
    check_guest("3b", &s, 0x7u);
    want_nothing(&w);
    w.readbacks = 3u;
    w.batches = 2u;
    w.packed = 2u;                              /* TC and TD are both 16-bit */
    w.packed_pixels = (uint64_t)TC_W * TC_H + (uint64_t)TD_W * TD_H;
    w.copied = 1u;                              /* TE */
    w.bytes = (uint64_t)TC_SLICE + (uint64_t)TD_SLICE + (uint64_t)TE_SLICE;
    check_counters("3b", &b, &a, &w);

    wc = gate_write_of("vkCmdCopyBuffer", 0u, 0u);
    snprintf(what, sizeof what, "3b exactly one retry: the flush was entered twice, and TC's pack "
                                "path is recorded once per attempt");
    check(c.begins == 2u && c.nbegins == 2u && gate_count_writes("vkCmdDispatch (pack kernel, "
          "shader writes)", 0u) == 2u && gate_count_writes("vkCmdCopyBuffer", 0u) == 2u
          && c.ds_allocs == 4u && c.ds_failures == 1u && wc != NULL, what,
          "recordings begun %u, TC dispatches %u, TC buffer copies %u, pack-set attempts %u "
          "(failures %u)", c.begins, gate_count_writes("vkCmdDispatch (pack kernel, shader "
          "writes)", 0u), gate_count_writes("vkCmdCopyBuffer", 0u), c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "3b the slot was free again (pending 0, npending 0, fence and pool "
                                "reset) before the retry recorded");
    check(c.nbegins == 2u && c.beg[1].rec == 2u && c.beg[1].npending == 0u
          && c.beg[1].pending0 == 0 && c.beg[1].recording0 == 0
          && c.beg[1].fence_resets >= 1u && c.beg[1].pool_resets >= 1u, what,
          "at the retry's recording #%u: npending %u, slot 0 pending %d recording %d, fence "
          "resets %u, pool resets %u", c.nbegins == 2u ? c.beg[1].rec : 0u,
          c.nbegins == 2u ? c.beg[1].npending : 0u, c.nbegins == 2u ? c.beg[1].pending0 : -1,
          c.nbegins == 2u ? c.beg[1].recording0 : -1, c.nbegins == 2u ? c.beg[1].fence_resets : 0u,
          c.nbegins == 2u ? c.beg[1].pool_resets : 0u);

    gate_decide("3b both passes, scenario B", &o);
    gate_order_checks("3b both passes, scenario B", &o, 1);
    snprintf(what, sizeof what, "3b the reuse this scenario is about -- the abandoned pack-output "
                                "buffer copy and TE's copy -- appears in both recordings");
    check(o.reuses == 5u && o.unordered == 0u, what,
          "overlapping reuses %u, unordered %u (the abandoned buffer copy against TE's copy, the "
          "same pair across the retry, TC's dispatches against each other, and the retry's TC/TD "
          "copies over what the first pass left)", o.reuses, o.unordered);

    snprintf(what, sizeof what, "3b the boundary counted nothing as left unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed);
    note("each target's rectangle is compared with the bytes the fixture supplied for it when the "
         "batch that published it passed its fence -- which is what a real device has there at that "
         "instant; the gate's decision is the recorded stream, not these bytes");

    verdict("3b the recovery control, scenario B", chk, bad);
}

/* ── 3c. the recovery control, scenario C ──────────────────────────────────────────── */

static void prop3c(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecWrite *wd;
    int rc;
    char what[170];
    int chk = g_checks, bad = g_check_failures;

    section("3c. the recovery control, scenario C: TX, TC2 and TD2 publish on the retry");
    note("the same boundary on scenario C, whose abandoned recording holds a pack buffer READ -- "
         "the retry records the same dispatches and copies again, so the WAR and WAW classes are "
         "both decided across the two passes as well");

    scenario_c(&s);
    c.ds_fail_at = 2;                           /* TD2's set in the first attempt only */
    stats(&b);
    rc = vb_targets_flush_recover(s.t, s.n);
    stats(&a);

    snprintf(what, sizeof what, "3c the boundary published everything: its own return value says so");
    check(rc == 1, what, "vb_targets_flush_recover returned %d (0 would mean a target stayed "
                         "dirty)", rc);
    check_dirty("3c", &s, 0u);
    check_guest("3c", &s, 0xFu);
    want_nothing(&w);
    w.readbacks = 4u;
    w.batches = 2u;
    w.packed = 3u;                              /* TC2, TD2 and TF are all 16-bit */
    w.packed_pixels = (uint64_t)TC2_W * TC2_H + (uint64_t)TD2_W * TD2_H + (uint64_t)TF_W * TF_H;
    w.copied = 1u;                              /* TX */
    w.bytes = (uint64_t)TC2_SLICE + (uint64_t)TX_SLICE + (uint64_t)TD2_SLICE
              + (uint64_t)TF_SLICE;
    check_counters("3c", &b, &a, &w);

    wd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 0u, 0u);
    snprintf(what, sizeof what, "3c exactly one retry: the flush was entered twice, and TC2's pack "
                                "path is recorded once per attempt");
    check(c.begins == 2u && c.nbegins == 2u && gate_count_writes("vkCmdDispatch (pack kernel, "
          "shader writes)", 0u) == 2u && gate_count_writes("vkCmdCopyBuffer", 0u) == 2u
          && c.dispatches == 4u && c.ds_allocs == 5u && c.ds_failures == 1u && wd != NULL, what,
          "recordings begun %u, TC2 dispatches %u, TC2 buffer copies %u, all dispatches %u, "
          "pack-set attempts %u (failures %u)", c.begins, gate_count_writes("vkCmdDispatch (pack "
          "kernel, shader writes)", 0u), gate_count_writes("vkCmdCopyBuffer", 0u), c.dispatches,
          c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "3c the slot was free again (pending 0, npending 0, fence and pool "
                                "reset) before the retry recorded");
    check(c.nbegins == 2u && c.beg[1].rec == 2u && c.beg[1].npending == 0u
          && c.beg[1].pending0 == 0 && c.beg[1].recording0 == 0
          && c.beg[1].fence_resets >= 1u && c.beg[1].pool_resets >= 1u, what,
          "at the retry's recording #%u: npending %u, slot 0 pending %d recording %d, fence "
          "resets %u, pool resets %u", c.nbegins == 2u ? c.beg[1].rec : 0u,
          c.nbegins == 2u ? c.beg[1].npending : 0u, c.nbegins == 2u ? c.beg[1].pending0 : -1,
          c.nbegins == 2u ? c.beg[1].recording0 : -1, c.nbegins == 2u ? c.beg[1].fence_resets : 0u,
          c.nbegins == 2u ? c.beg[1].pool_resets : 0u);

    gate_decide("3c both passes, scenario C", &o);
    gate_order_checks("3c both passes, scenario C", &o, 1);
    snprintf(what, sizeof what, "3c across both passes every write/write AND write/read reuse is "
                                "ordered");
    check(o.unordered == 0u && o.war_unordered == 0u && o.reuses == 7u && o.war_reuses == 3u, what,
          "write/write reuses %u (unordered %u), write/read pairs %u (unordered %u) -- the "
          "abandoned read against the next group's dispatch and against the retry's own "
          "dispatches, plus the staging bytes each pass rewrites", o.reuses, o.unordered,
          o.war_reuses, o.war_unordered);

    snprintf(what, sizeof what, "3c the boundary counted nothing as left unpublished");
    check(g.st.readback_incomplete == 0u && a.incomplete == 0u && g.ok == 1 && g.failed == 0, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, g.ok %d, g.failed %d",
          (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete, g.ok,
          g.failed);

    verdict("3c the recovery control, scenario C", chk, bad);
}

/* ── 4d. the permanent-failure form, scenario C ────────────────────────────────────── */

/* The form that both records the abandoned pack path AND stops the back end: TD2's set is the
 * injected failure, so attempt 1 abandons the group with TC2's dispatch, its barrier and the
 * buffer copy that reads g.pack_buf recorded, while TF's own set (the third allocation) still
 * succeeds so TF publishes and the next group's dispatch writes the pack buffer bytes the
 * abandoned read read; and then EVERY allocation from the retry's on fails, so the retry abandons
 * before recording anything and the boundary stops with the three unpublished targets counted. */
static void prop4d(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecRead *rd;
    const RecWrite *fd;
    unsigned i, retry_writes = 0;
    char ra[190], rb[190], what[170];
    int chk = g_checks, bad = g_check_failures;

    section("4d. the permanent-failure form, scenario C: the WAR pair is recorded AND the boundary "
            "stops");
    note("driven through vb_flip with TD2's set failing and every allocation from the retry's on "
         "failing too");

    scenario_c(&s);
    c.ds_fail_at = 2;                           /* TD2's set in attempt 1 */
    c.ds_fail_from = 4;                         /* and every set from the retry's on */
    stats(&b);
    vb_flip();
    stats(&a);

    snprintf(what, sizeof what, "4d the boundary stopped the back end instead of moving on");
    check(g.ok == 0 && g.failed == 1, what, "g.ok %d, g.failed %d", g.ok, g.failed);
    snprintf(what, sizeof what, "4d the three targets it could not publish are counted");
    check(g.st.readback_incomplete == 3u && a.incomplete == 3u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, pack-set attempts %u (failures "
          "%u)", (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete,
          c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "4d no frame advanced and no target was marked stale");
    check(g.frame == 0u && s.t[0]->stale == 0 && s.t[1]->stale == 0 && s.t[2]->stale == 0
          && s.t[3]->stale == 0, what, "g.frame %u, stale TC2 %d, TX %d, TD2 %d, TF %d", g.frame,
          s.t[0]->stale, s.t[1]->stale, s.t[2]->stale, s.t[3]->stale);
    check_dirty("4d", &s, 3u);
    check_guest("4d", &s, 0x8u);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.packed = 1u;
    w.packed_pixels = (uint64_t)TF_W * TF_H;
    w.bytes = (uint64_t)TF_SLICE;
    check_counters("4d", &b, &a, &w);

    gate_decide("4d the whole flip, scenario C", &o);
    gate_order_checks("4d the whole flip, scenario C", &o, 1);

    rd = gate_read_of(0u, 0u);
    fd = gate_write_of("vkCmdDispatch (pack kernel, shader writes)", 3u, 0u);
    read_range_of(rd, ra, sizeof ra);
    range_of(fd, rb, sizeof rb);
    snprintf(what, sizeof what, "4d the write-after-read pair is the one attempt 1 recorded, and it "
                                "is ordered");
    check(rd && fd && o.war_reuses == 1u && o.war_unordered == 0u
          && gate_ordering_barrier_war(rd, fd) != NULL, what,
          "WAR pairs %u, war_unordered %u; read: %s; later write: %s", o.war_reuses,
          o.war_unordered, ra, rb);

    for (i = 0; i < c.nwrites; i++)
        if (c.writes[i].rec >= 2u)
            retry_writes++;
    snprintf(what, sizeof what, "4d the retry recorded no access at all: its failure comes first");
    check(retry_writes == 0u && c.nreads == 2u, what,
          "writes in the retry's recording %u, reads recorded over the whole flip %u (the two pack "
          "paths attempt 1 recorded: TC2's and the one TF's own copy reads through)",
          retry_writes, c.nreads);

    verdict("4d the permanent-failure form, the WAR pair recorded and the stop", chk, bad);
}

/* ── 4. the permanent-failure control, through the real flip ───────────────────────── */

static void prop4(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    unsigned i, retry_submitted = 0, t0_copies = 0;
    char what[150];
    int chk = g_checks, bad = g_check_failures;

    section("4. the permanent-failure control: the flip stops, and nothing is unordered");
    note("driven through vb_flip -- the real frame boundary -- with EVERY pack-set allocation "
         "failing, so the flush and its one bounded retry both leave T0 and T1 unpublished");

    scenario(&s, 0);
    c.ds_fail_all = 1;
    stats(&b);
    vb_flip();
    stats(&a);

    snprintf(what, sizeof what, "4 the boundary stopped the back end instead of moving on");
    check(g.ok == 0 && g.failed == 1, what, "g.ok %d, g.failed %d", g.ok, g.failed);
    snprintf(what, sizeof what, "4 the two targets it could not publish are counted");
    check(g.st.readback_incomplete == 2u && a.incomplete == 2u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, pack-set attempts %u (all "
          "failing: %u)", (unsigned long long)g.st.readback_incomplete,
          (unsigned long long)a.incomplete, c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "4 no frame advanced and no target was marked stale");
    check(g.frame == 0u && s.t[0]->stale == 0 && s.t[1]->stale == 0 && s.t[2]->stale == 0, what,
          "g.frame %u, stale T0 %d, T1 %d, %s %d", g.frame, s.t[0]->stale, s.t[1]->stale,
          s.g[2].name, s.t[2]->stale);

    check_dirty("4", &s, 2u);
    snprintf(what, sizeof what, "4 T0's and T1's rectangles are their pre-call guest bytes");
    check(marker_bad(&s, 0u) == 0u && marker_bad(&s, 1u) == 0u, what,
          "T0 %zu of %llu bytes differ, T1 %zu of %llu", marker_bad(&s, 0u),
          (unsigned long long)T0_SLICE, marker_bad(&s, 1u), (unsigned long long)T1_SLICE);
    snprintf(what, sizeof what, "4 the third target still holds its own correct published "
                                "rectangle");
    check(published_bad(&s, 2u) == 0u, what, "%zu of %llu bytes differ from the bytes the "
          "fixture supplied for its own slice", published_bad(&s, 2u),
          (unsigned long long)T2_SLICE);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.copied = 1u;
    w.bytes = (uint64_t)T2_SLICE;
    check_counters("4", &b, &a, &w);
    check_guest("4", &s, 0x4u);

    gate_decide("4 the whole flip", &o);
    gate_order_checks("4 the whole flip", &o, 1);

    /* The retry's own abandoned recording was never submitted: the stop happens first, so the
       bytes it would have written never reach the device. */
    for (i = 0; i < c.nsubs; i++)
        if (c.subs[i].rec >= 2u)
            retry_submitted++;
    for (i = 0; i < c.nwrites; i++)
        if (c.writes[i].target == 0u)
            t0_copies++;
    snprintf(what, sizeof what, "4 the retry's abandoned copy was recorded but never submitted");
    check(retry_submitted == 0u && c.submits == 1u && t0_copies == 2u, what,
          "submissions %u (of the retry's recording %u), T0 copies recorded %u, writes %u",
          c.submits, retry_submitted, t0_copies, c.nwrites);

    verdict("4 the permanent-failure control", chk, bad);
}

/* ── 4b/4c. the permanent-failure forms, scenario B ─────────────────────────────────── */

/* 4b: every pack-set allocation fails, which is property 4's form on scenario B. The first
 * failure is TC's own set, so the abandoned group records no write at all: the property is that
 * the boundary still stops with TC and TD counted, TE still publishes its own rectangle, and
 * nothing is unordered. The reuse this scenario exists for cannot appear here, and the case says
 * so instead of reporting a "0 of 0" as an ordered reuse. */
static void prop4b(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    char what[160];
    int chk = g_checks, bad = g_check_failures;

    section("4b. the permanent-failure form, scenario B: every pack-set allocation fails");
    note("driven through vb_flip with EVERY allocation failing; TC's set is the first one, so the "
         "abandoned group records nothing and the only write of the pass is TE's own copy");

    scenario_b(&s);
    c.ds_fail_all = 1;
    stats(&b);
    vb_flip();
    stats(&a);

    snprintf(what, sizeof what, "4b the boundary stopped the back end instead of moving on");
    check(g.ok == 0 && g.failed == 1, what, "g.ok %d, g.failed %d", g.ok, g.failed);
    snprintf(what, sizeof what, "4b the two targets it could not publish are counted");
    check(g.st.readback_incomplete == 2u && a.incomplete == 2u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, pack-set attempts %u (all "
          "failing: %u)", (unsigned long long)g.st.readback_incomplete,
          (unsigned long long)a.incomplete, c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "4b no frame advanced and no target was marked stale");
    check(g.frame == 0u && s.t[0]->stale == 0 && s.t[1]->stale == 0 && s.t[2]->stale == 0, what,
          "g.frame %u, stale TC %d, TD %d, TE %d", g.frame, s.t[0]->stale, s.t[1]->stale,
          s.t[2]->stale);
    check_dirty("4b", &s, 2u);
    check_guest("4b", &s, 0x4u);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.copied = 1u;
    w.bytes = (uint64_t)TE_SLICE;
    check_counters("4b", &b, &a, &w);

    gate_decide("4b the whole flip, scenario B", &o);
    gate_order_checks("4b the whole flip, scenario B", &o, 0);
    snprintf(what, sizeof what, "4b no pack write was recorded at all: the failure precedes the "
                                "dispatch");
    check(c.dispatches == 0u && c.bufcopy == 0u
          && c.ds_fail_seq_first < (c.nwrites ? c.writes[0].seq : 0u), what,
          "pack dispatches %u, buffer copies %u, the first failed allocation at #%u, the first "
          "write at #%u", c.dispatches, c.bufcopy, c.ds_fail_seq_first,
          c.nwrites ? c.writes[0].seq : 0u);

    verdict("4b the permanent-failure form, every allocation failing", chk, bad);
}

/* 4c: the permanent-failure form that DOES record the abandoned pack path -- the failure
 * persists at TD's set, so every attempt abandons the group after TC's dispatch and its buffer
 * copy into g.rb_buf. That is the pack-path write class under the stop: the deciding reuse is
 * present, TC and TD are counted incomplete, and the boundary stops. */
static void prop4c(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    const RecWrite *wc, *we;
    const RecBar *r;
    unsigned i, retry_submitted = 0;
    char ra[190], rb[190], what[160];
    int chk = g_checks, bad = g_check_failures;

    section("4c. the permanent-failure form, scenario B: the failure persists at TD's set");
    note("driven through vb_flip with every allocation from the SECOND on failing: attempt 1 "
         "abandons after TC's dispatch and its buffer copy, publishes TE, and the retry abandons "
         "before recording anything, so the boundary stops");

    scenario_b(&s);
    c.ds_fail_from = 2;
    stats(&b);
    vb_flip();
    stats(&a);

    snprintf(what, sizeof what, "4c the boundary stopped the back end instead of moving on");
    check(g.ok == 0 && g.failed == 1, what, "g.ok %d, g.failed %d", g.ok, g.failed);
    snprintf(what, sizeof what, "4c the two targets it could not publish are counted");
    check(g.st.readback_incomplete == 2u && a.incomplete == 2u, what,
          "g.st.readback_incomplete %llu, stats incomplete %llu, pack-set attempts %u (failures "
          "%u)", (unsigned long long)g.st.readback_incomplete, (unsigned long long)a.incomplete,
          c.ds_allocs, c.ds_failures);
    snprintf(what, sizeof what, "4c no frame advanced and no target was marked stale");
    check(g.frame == 0u && s.t[0]->stale == 0 && s.t[1]->stale == 0 && s.t[2]->stale == 0, what,
          "g.frame %u, stale TC %d, TD %d, TE %d", g.frame, s.t[0]->stale, s.t[1]->stale,
          s.t[2]->stale);
    check_dirty("4c", &s, 2u);
    check_guest("4c", &s, 0x4u);
    want_nothing(&w);
    w.readbacks = 1u;
    w.batches = 1u;
    w.copied = 1u;
    w.bytes = (uint64_t)TE_SLICE;
    check_counters("4c", &b, &a, &w);

    gate_decide("4c the whole flip, scenario B", &o);
    gate_order_checks("4c the whole flip, scenario B", &o, 1);

    wc = gate_write_of("vkCmdCopyBuffer", 0u, 0u);
    we = gate_write_of(NULL, 2u, 0u);
    range_of(wc, ra, sizeof ra);
    range_of(we, rb, sizeof rb);
    snprintf(what, sizeof what, "4c the deciding reuse is the abandoned pack-output buffer copy "
                                "and TE's copy, in one recording");
    check(o.reuses == 1u && o.unordered == 0u && wc && we && wc->rec == we->rec, what,
          "reuses %u, unordered %u, same recording %d; abandoned: %s; TE: %s", o.reuses,
          o.unordered, (wc && we && wc->rec == we->rec), ra, rb);
    r = (wc && we) ? gate_ordering_barrier(wc, we) : NULL;
    snprintf(what, sizeof what, "4c the barrier between them is the broad abandonment barrier");
    check(r && r->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->dst == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
          && r->srca == VK_ACCESS_MEMORY_WRITE_BIT
          && r->dsta == (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT), what, "%s",
          r ? "srcStage/access and dstStage/access as required"
            : "NO ordering barrier between them at all");

    for (i = 0; i < c.nsubs; i++)
        if (c.subs[i].rec >= 2u)
            retry_submitted++;
    snprintf(what, sizeof what, "4c the retry's recording is never submitted: the stop precedes it");
    check(retry_submitted == 0u && c.submits == 1u && c.begins == 2u, what,
          "submissions %u (of the retry's recording: %u), recordings begun %u", c.submits,
          retry_submitted, c.begins);

    verdict("4c the permanent-failure form, the failure persisting", chk, bad);
}

/* ── 5. the success control ────────────────────────────────────────────────────────── */

static void prop5(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    GateOrder o;
    RbWant w;
    char what[150];
    int chk = g_checks, bad = g_check_failures;

    section("5. the success control: the three targets publish in exactly two batches");
    note("no injection: T0 and T1 fill the cap as one group, the third target is the next, and "
         "each group is one recording, one submit, one fence wait and one invalidation");

    scenario(&s, 0);
    stats(&b);
    vb_targets_flush(s.t, s.n);
    stats(&a);

    snprintf(what, sizeof what, "5 the three targets published in exactly two batches");
    check(c.begins == 2u && c.resets == 2u && c.submits == 2u && c.waits == 2u
          && c.invalidates == 2u && c.submitted[0] == g.slots[0].cmd
          && c.submitted[1] == g.slots[0].cmd && c.ds_failures == 0u, what,
          "recordings %u, resets %u, submits %u, fence waits %u, invalidations %u, copies' pack "
          "attempts %u", c.begins, c.resets, c.submits, c.waits, c.invalidates, c.ds_allocs);
    check_dirty("5", &s, 0u);
    check_guest("5", &s, 0x7u);
    want_nothing(&w);
    w.readbacks = 3u;
    w.batches = 2u;
    w.packed = 1u;
    w.packed_pixels = (uint64_t)T1_W * T1_H;
    w.copied = 2u;
    w.bytes = (uint64_t)T0_SLICE + (uint64_t)T1_SLICE + (uint64_t)T2_SLICE;
    check_counters("5", &b, &a, &w);

    gate_decide("5 the success control", &o);
    gate_order_checks("5 the success control", &o, 1);
    snprintf(what, sizeof what, "5 the overlap between the groups is real and ordered by the "
                                "submission and fence wait between them");
    check(o.reuses == 1u && o.unordered == 0u, what,
          "overlapping reuses %u, unordered %u -- group 2's copy of the third target rewrites the "
          "staging bytes group 1's T0 copy wrote (both start at their group's slice offset 0), and "
          "group 1 really was submitted and waited for before group 2 recorded",
          o.reuses, o.unordered);

    snprintf(what, sizeof what, "5 exactly one staging pair was made, and nothing was freed");
    check(c.buffers == 2u && c.allocs == 2u && c.maps == 1u && c.frees == 0u
          && c.buffer_destroys == 0u && g.ngarbage == 0u && g.rb_buf != VK_NULL_HANDLE
          && g.pack_buf != VK_NULL_HANDLE && g.rb_ptr == g_stage
          && g.rb_cap == VB_READBACK_MAX, what,
          "vkCreateBuffer %u, vkAllocateMemory %u, vkMapMemory %u, frees %u, destroys %u, held "
          "garbage %llu, rb %p/%p, pack %p, rb_ptr %p (arena %p), capacity %llu", c.buffers,
          c.allocs, c.maps, c.frees, c.buffer_destroys, (unsigned long long)g.ngarbage,
          (void *)g.rb_buf, (void *)g.rb_mem, (void *)g.pack_buf, (void *)g.rb_ptr,
          (void *)g_stage, (unsigned long long)g.rb_cap);

    verdict("5 the success control", chk, bad);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ───────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)g_guest;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    (void)backend;
}

int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    (void)tex; (void)argb_out;
    return 0;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    (void)tex;
    return 0;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* ── The run ────────────────────────────────────────────────────────────────────────── */

int main(void)
{
    unsigned i;

    printf("vk_readback_ordering_gate: the readback publication's ordering at the point a group "
           "is abandoned,\nagainst the toolkit's own nv2a_vk_backend.c\n");
    printf("no GPU, no ICD and no skip path: every Vulkan entry point the paths reach is a "
           "controlled stub that\nrecords the command stream, and the staging buffer vkMapMemory "
           "hands back is the fixture's own bytes\n");

    g_guest = (uint8_t *)malloc(GATE_GUEST);
    g_stage = (uint8_t *)malloc(GATE_STAGE);
    g_row = (uint8_t *)malloc(GATE_ROW);
    for (i = 0; i < VB_SLOTS; i++)
        g_stream[i] = (uint8_t *)malloc(VB_SLOT_STREAM);
    if (!g_guest || !g_stage || !g_row
        || !g_stream[0] || !g_stream[1] || !g_stream[2]) {
        fprintf(stderr, "FAIL: vk_readback_ordering_gate: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();

    prop0();
    prop1a();
    prop1b();
    prop1c();
    prop1d();
    prop2();
    prop3();
    prop3b();
    prop3c();
    prop4();
    prop4b();
    prop4c();
    prop4d();
    prop5();

    for (i = 0; i < VB_SLOTS; i++)
        free(g_stream[i]);
    free(g_row);
    free(g_stage);
    free(g_guest);

    printf("\n");
    printf("vk_readback_ordering_gate: %u overlapping reuses of the staging bytes across the "
           "properties, %u of them unordered; %u write-after-read pairs on the pack buffer, "
           "%u of them unordered\n", g_reuses, g_unordered, g_war_reuses, g_war_unordered);
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_readback_ordering_gate: %d of %d properties failed (%d of %d "
                        "checks failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_readback_ordering_gate: %d properties, %d checks -- a group that could not "
           "stage every target is ordered before the next group reuses its staging offsets, and "
           "the pack buffer's read and the next group's write of the same bytes are ordered "
           "too, so no overlapping device access is left unordered\n", g_props, g_checks);
    return 0;
}
