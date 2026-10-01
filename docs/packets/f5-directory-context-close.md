## F5-D2 — release the directory-search context on close

**Class:** owner-directed F5 chore — bounded code/test unit (the Advisor calls it a separate code
packet); **not a workflow change or discovery packet; CURRENT PACKET remains none, unchanged.**
**Contract revision:** r1   **Status:** APPROVED — implementation **IN PROGRESS** (RED **VERIFIED**, GREEN pending)
**Authority:** this unit is named by the **original owner prompt step 4** and sits inside the **F5
chore** in `plan-jsrf-bare-minimum.md` §13 — **the same title/link as that chore entry**, not a new
promoted packet. Workflow §5.8's class list and plan §3/§13 take precedence here; **no promotion is
claimed.**
**Starts from:** the F5 retry1 check loop. The guest re-creates `JSRF_CACHE_COMPLETE0N.CMP` about
994 times with status 0 and never reaches F6. The Advisor's D2 design ruling identifies a **likely
source candidate** — the toolkit's emulated directory search (`NtQueryDirectoryFile`) rather than the
marker-create path: `kernel_file.c` keeps a fixed table of 64 search contexts, and a successful first
query leaves its context marked "first query done", with **no cleanup on close**, so a later query
either matches a stale context (handle reuse → `STATUS_NO_MORE_FILES`) or finds no free slot
(`STATUS_INSUFFICIENT_RESOURCES`), which is **consistent with** an existing marker appearing absent.
**This is a source-level candidate, not proven guest causality**: it is **conditional** on handle reuse
or a full table, and D1 did not measure the table. **The loop's cause is UNPROVEN.**
**Baseline:** game `d1f30e1`, toolkit `a71f937`
**Senior-call budget:** **within the existing F4→F6 W14 accounting** — this unit is **not** exempt from
W14, and the parent's authorisation is a **coordination/owner-boundary** basis, **not** a budget
exemption. **W14's clock and packet-count conditions both apply** (see Status and gating).
**Design authority:** Advisor child `63c4869f-3689-41b2-89b8-f5429f8b5927` (route
`claude`/`claude-opus-5-5` @ `high`, parent-pinned). Full design text verbatim:
`logs/workers/f5-directory-context-d2-design-verbatim.md`; ruling + reply 3 + harness ACK verbatim in
`docs/reviews/rulings/f4-submission-capacity.md` (F5 directory-probe appendix).

### Status and gating

- **APPROVED / implementation IN PROGRESS.** **RED VERIFIED on `a71f937`** (both modes, below);
  **GREEN pending**; **NOT ACCEPTED** until GREEN, tests and an allowed run.
- **Verified RED (parent-read):** **2/2 CTest tests failed** — `xbox_dir_context_release_direct` and
  `xbox_dir_context_release_bridge` — **rc 8, 0.29 s**; in each mode **64 opened / 64 queried**, the
  **65th query FAILED with status `0x80000006`** (`STATUS_NO_MORE_FILES`), and **churn failed at round
  1 of 200** with the same status. **Artifacts, cited as actually read:** `red-direct.log` (parent read
  its initial ~70 lines) and `red-ctest.log` (load-bearing counts in both modes); the newer
  `red-direct-raw.txt` / `red-bridge-raw.txt` are **UTF-16**, their read **failed**, and they are
  **pending UTF-8 conversion** — not cited as parent-read. The earlier **8-run 4-fail/4-pass result is superseded and NOT an accepted
  RED** (`RestartScan 1` masked the stale-context path); a worker claim of 5/5 per mode is
  **worker-reported, not parent-verified**.
- **Cause is NOT proven** by the RED: it shows the observable failure and its status, **not** that the
  context leak is the actual cause of the guest loop.
- **Test first.** The test is written before the fix and **fails on `a71f937`**.
- **No guest run until GREEN.** No seeding. No change to `kernel_file.c:234`.
- **W14 (plan §3 rule — "3 packets or 4 h without moving the horizon or an accepted critical-path
  finding → one Advisor ceiling call"):** the **ceiling stays at the original 4 h, `14:32:32` UTC**,
  with **no reset** from D1 or from the source candidate alone; a **RED-then-GREEN D2 that later moves
  the guest past F5** would be the candidate for a reset, and a source candidate alone is not.
  **D2 is ONE pending candidate unit, not three** (Advisor budget ACK, verbatim in the ruling
  appendix), so the packet-count threshold is **not** reached. The ACK's own caveat: it **did not read
  the W14 text itself** — it confirms consistency with its history, **not** an independent rules audit;
  the **authority is plan §3**, which the parent read. Elapsed time alone still cannot establish that
  the count condition is clear. **As of `13:14:39` the ceiling `14:32:32` is still unreset.**

### Guard clarification (Advisor, verbatim in the ruling appendix)

- **Invalid/synthetic handles:** `xbox_dir_context_release` **returns void and changes no return
  value**.
  - In `xbox_NtClose`, call it **only inside the existing
    `if (Handle && Handle != INVALID_HANDLE_VALUE)` block** — the `STATUS_INVALID_HANDLE` path stays
    untouched.
  - In `bridge_NtClose`, call it **only inside the existing
    `if (raw_handle && raw_handle != 0xDEAD0001u && raw_handle != 0xBEEF0010u)` block**, after
    `bridge_take_handle`, and only when `h` is neither NULL nor `INVALID_HANDLE_VALUE`. **`g_eax = 0`
    stays as it is.**
  - **The helper itself must also no-op on NULL/INVALID** (defence in depth: cheap, and it means no
    empty-slot match on NULL).
- **IMPORTANT:** Windows `find_or_create_dir_context` treats **`find_handle == NULL` as a free slot**,
  so release **must also clear `file_handle`** — otherwise a stale `file_handle` lingers in a slot that
  looks free. Harmless for lookup (which requires `find_handle != NULL`), but cleared so the table
  state is unambiguous.
- **Optional guard test:** closing a synthetic **`0xDEAD0001`** through the bridge returns 0 and leaves
  the table unchanged — cheap if the harness exists.

### D1 result carried in (context only — no causality claim)

D1 was **INCONCLUSIVE**: `s_dir_contexts` live VA `0x00007FF708EB7780` has **0 containing ranges** in
the retry1 `process.dmp`, **0/4928** readable 8-byte slots, dump coverage **0.96%** of the exe image
(113 ranges; `.data` 1.31%, `.text` 0.01%; `.rdata`/`.pdata`/`.rsrc`/`.reloc` absent). The pre-check's
"0 overlap" line is **superseded**. Raw: `logs/workers/f5-retry1/symbol-locate-utf8.txt`,
`dump-coverage-utf8.txt`. **D1 did not confirm the mechanism by measurement.** The **corrected RED
below confirms a fixture-level lifetime failure** — the directory-context table leaks across
open/query/close — and **not the guest loop's cause**; this packet must not claim D1, or the RED,
proved guest causality.

### Load-bearing premises (W10)

**All source premises below are stated against the `a71f937` baseline** — the toolkit revision this
unit starts from. Line references are to that revision's source; the live tree now carries
**in-progress fix edits**, which does **not** falsify the baseline statements, and **no cleanup exists
at that baseline** on the close path.

- **`s_dir_contexts` is a fixed 64-entry table with no cleanup ON CLOSE.** Source: `kernel_file.c`
  `590-623` (table + `s_dir_cs` lazy init) and `625-729` (`xbox_NtQueryDirectoryFile`). Verified by
  reading the toolkit source. **Qualified:** the context *is* cleaned up when a later "next file" call
  **fails** — what is absent is cleanup on the **close** path, which is the gap this packet addresses.
- **`bridge_NtClose` does not go through `xbox_NtClose`.** Source: `kernel_bridge.c:734-752`
  (`bridge_take_handle` then `CloseHandle`). **This is why hooking only `xbox_NtClose` would give a
  false GREEN on the guest path.**
- **`xbox_NtClose` has two sites.** `kernel_file.c:316` and `:907`, each with a valid-handle check
  before `CloseHandle`.
- **The Windows branch is the live one** (`#if _WIN32`, lines 70–732); the POSIX branch
  (`1128-1237`) has its own table and needs the same review.

### Change (from the design ruling, numbered)

1. **HOOK.** Add one release function to `kernel_file.c` with a body in each branch (Windows and
   POSIX), e.g. `void xbox_dir_context_release(HANDLE h)`, declared in `kernel.h`. Call it at **both**
   sites:
   a. `xbox_NtClose`, at **both `:316` and `:907`** — inside the existing valid-handle check,
      **before `CloseHandle`**.
   b. `bridge_NtClose` (`kernel_bridge.c:747-749`) — after `bridge_take_handle` and before
      `CloseHandle(h)`, only when `h` is valid.
   **Release BEFORE the host close**; reversed order would let another thread receive the reused
   handle value and have its new context wiped.
2. **RELEASE SEMANTICS.** Take `s_dir_cs` using the existing lazy-init helper.
   - **Windows:** for every slot whose `file_handle == h` (regardless of `find_handle`), `FindClose`
     the find handle if non-NULL and not `INVALID_HANDLE_VALUE`, then zero the slot
     (`file_handle`, `find_handle`, `first_done`).
   - **POSIX:** for every slot whose `handle == h`, `closedir` if non-NULL, then set `handle` and
     `dir` to NULL.
   - **Invalid, NULL or synthetic handles** (`0xDEAD0001`, `0xBEEF0010`): **no-op**, keeping the
     existing return values.
   - **Do not change** the lookup or query semantics, the dot-directory filtering, the
     `FindNextFile`-failure cleanup, or line `:234`.
3. **RISKS — recorded, NOT fixed here.** The lazy `InitializeCriticalSection` (`s_dir_cs_init` flag)
   is a pre-existing race; on Windows the query uses `ctx` outside the lock after lookup (a guest
   closing a handle while another thread queries it is guest misuse); before acceptance, **grep for
   any other path that `CloseHandle`s a taken token** — each needs the same call.

### Experiments / RED (deterministic, actual Windows build, separate test process per ctest)

Fixture: **65 distinct directories, each containing exactly one file `M.CMP`** (not one directory
queried 64 times).

- **Direct-API test:** open **64 distinct directories** with **`xbox_NtOpenFile` only** using
  `DIRECTORY_FILE`, keeping all handles open; query each once with the exact mask `M.CMP` — every
  query must succeed. Close all 64 with `xbox_NtClose`. Open a **65th distinct directory** and query
  with `M.CMP`. (`NtCreateFile` is **not** used — it was an optional alternative in the approved design
  and is **not needed**.)
  On `a71f937` the 65th query is **expected to fail either way** (handle reuse → stale `first_done` →
  `FindNextFile` → `NO_MORE_FILES`; no reuse → no free slot → `INSUFFICIENT_RESOURCES`), so the test
  **does not depend on handle reuse**. **MEASURED: it failed with `0x80000006` (`NO_MORE_FILES`) in
  both modes** — i.e. the reuse path was the one taken. Assert `STATUS_SUCCESS` and **record the actual
  failing status** in the RED record.
  Add a **churn assertion**: 200 rounds of open, query, close — each must succeed.
- **Bridge-path test (REQUIRED — no direct-only GREEN):** run the same 64/65 sequence through
  `bridge_NtOpenFile`, `bridge_NtQueryDirectoryFile` and `bridge_NtClose`, with guest-memory handle,
  IOSB and `ANSI_STRING` mask. **If no bridge/kernel-thunk harness exists, the packet must add the
  minimal one or record bridge coverage as NOT verified — which means NOT ACCEPTED.** `bridge_take_handle`
  must have resolved the token to the native `HANDLE` the context is keyed by; the test proves this
  end to end.
- **Use `NtOpenFile`, NOT `NtCreateFile` (Advisor harness-plan ACK, verbatim in the ruling appendix).**
  The guest path is **`NtOpenFile`** — thunk **ordinal 202**, from the original raw at `145F08-145F20`;
  `bridge_NtOpenFile` (`kernel_bridge.c:3340-3353`) forwards to the same `bridge_create_file_impl` with
  **disposition 1 (`FILE_OPEN`)** and **allocation 0**. The two are functionally equivalent, but an
  `NtOpenFile` wrapper costs one more same-shape seam wrapper **and matches the guest exactly**, so a
  **fourth wrapper `xbox_test_bridge_NtOpenFile` is REQUIRED** for the RED/GREEN bridge test; **the
  `NtCreateFile` wrapper is optional — drop it if not needed for fixture setup**.
- **Exact guest arguments (from the ACK):** **DesiredAccess `0x100001`**, **ShareAccess `3`**,
  **OpenOptions `0x4021`** (`DIRECTORY_FILE | SYNCHRONOUS_IO_NONALERT | OPEN_FOR_BACKUP_INTENT`);
  query with **FileInformationClass `1`**, **Length `0x148`**, **RestartScan `0`**, and an
  **`ANSI_STRING` mask `M.CMP` in guest memory**.
- **Approved as stated:** the toolkit fixture, the existing `jsrf_test_write_stack` seam, wrappers in
  scope, POSIX cleanup compiled if a POSIX build exists (else "reviewed"), and **no further design
  changes**. **RED ONLY for now, parent-authorised — no production change until the parent reviews
  both RED statuses.**

### Acceptance

- **RED recorded on `a71f937`:** both tests fail and the statuses are recorded.
- **GREEN after the fix.**
- **Full toolkit ctest** and game **`just test` / `just check`** pass.
- **POSIX branch:** compile it if a POSIX build exists; otherwise record "POSIX reviewed, not
  compiled" — **that does not block the Windows packet**.
- **Push toolkit first, then the game record.**
- **No guest run until GREEN.** No seeding, no `:234` change. Smoke run and profile policy come later.

### Not in scope

Marker seeding; any change to line `:234`; the risks in item 3; the smoke/run policy; and any
causality claim from D1, which was inconclusive.
