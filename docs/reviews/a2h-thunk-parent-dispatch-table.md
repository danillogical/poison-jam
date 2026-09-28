# `A2h` — the named edge's PARENT: `sub_00153790` is entry 39 of a 40-entry DISPATCH TABLE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the writer trace's `O-OPEN` names **the third argument to the forwarding thunk
`sub_00153790`** as the smallest uncovered edge. **The Session traced that thunk's own provenance offline and
found its parent, which changes the edge's shape.**

---

## The DISPATCHER is LOCATED — and the table is a VTABLE installed at `object+0x00`

**The three references to the table base are all in `.text`:**

```
file 0x142246 -> .text VA 0x00152246
file 0x142296 -> .text VA 0x00152296
file 0x14230B -> .text VA 0x0015230B
```

**And the first of them is the decisive instruction:**

```
00152230  push esi
00152231  mov  esi, [esp + 8]          ; esi = the object
00152235  mov  eax, [esi + 0x608]      ; a REFERENCE COUNT
0015223B  dec  eax
0015223C  mov  [esi + 0x608], eax
00152242  jne  0x152261                ; not zero -> return
00152244  mov  dword ptr [esi], 0x1e1270   ; <== INSTALLS THE TABLE AS THE OBJECT'S VTABLE
0015224A  mov  ecx, [0x2650d8]
00152250  push esi
00152251  call 0x154f80
00152256  push esi
00152257  call 0x4a900
0015225C  add  esp, 4
0015225F  xor  eax, eax
00152261  pop  esi
00152262  ret  4
```

> ## **`mov dword ptr [esi], 0x1e1270` stores the table base into `object+0x00`.**
>
> **So `0x001E1270` is a VTABLE, and the object's first dword is its vtable pointer.** **The three references
> are the object's CONSTRUCTOR/DESTRUCTOR paths installing it.**

**This closes the structural question the Session opened:** **the table is not an anonymous dispatch array —
it is a C++-style vtable at offset `+0x00` of some object, installed when a reference count at `object+0x608`
reaches zero.** **And the reference-count check at `0x00152235`/`0x00152242` shows this is a
release/destruction path, not a construction path** — **it installs the vtable as the object is torn down.**

## What this means for the named edge

**The writer trace's edge was *"the third argument to `sub_00153790`."*** **Now:**

1. **`sub_00153790` is vtable entry 51 of the vtable at `0x001E1270`.**
2. **The vtable is installed at `object+0x00` by the object's teardown path.**
3. **So entry 51 is reached by a CALL THROUGH THE OBJECT'S VTABLE** — `call [obj+0]`-style dispatch, at
   **index 51**, **with whatever arguments the caller supplies.**

**The Session did NOT find the call site that indexes entry 51** — **that requires finding where a virtual
call with index 51 is made against an object carrying this vtable.** **So the edge is RELOCATED AGAIN, and
now it is a concrete, well-posed question:**

> **Where is a virtual call made through the vtable at `object+0x00` with index 51, and what is the third
> argument at that call?**

**The Session records this rather than claiming closure.** **But the search space has narrowed sharply: the
question moved from *"who calls an uncalled function?"* to *"where is index 51 dispatched?"*** — **and vtables
make that a structural question with a definite answer.**

## A structural observation the Session will NOT promote

**The vtable is installed on a TEARDOWN path (`dec [esi+0x608]` → `jne` → install).** **If entry 51 is only
reachable during teardown, then the `0x00199F45` store would run in a destruction context** — **which is
CONSISTENT with a type-confusion story where a filename reaches a callback slot during cleanup.** **But
"consistent with" is not evidence, and the Session explicitly does not promote it.** **Recorded as a
hypothesis for a successor to test, not as a finding.**

## The finding: the thunk has NO direct caller — it is a DISPATCH-TABLE ENTRY

### The table, fully characterized

| Property | Value |
|---|---|
| **Base** | **`0x001E1270`** |
| **Entries** | **52** |
| **Ends at** | **`0x001E133C`** — **which is `sub_00153790`, index 51, the LAST entry** |
| **Terminated by** | `0x001E126C` holds `0xC3000000` — **not a code VA**, so the table starts at `0x001E1270` |
| **Followed by** | high-entropy bytes from `0x001E1340` — **the table ends cleanly** |
| **References to the base** | **THREE** — **so the dispatcher is findable** |
| **Ordering** | **NOT ascending** → a vtable/dispatch table, not a switch table |
| **Targets** | cluster in `0x0014E1F0..0x00153850` — closely-spaced small functions |

**Sample entries:**

```
[  0] 0x001E1270 -> 0x00151D70      [ 44] 0x001E1320 -> 0x00153240
[  1] 0x001E1274 -> 0x00152150      [ 48] 0x001E1330 -> 0x00153850
[  4] 0x001E1280 -> 0x00151DB0      [ 50] 0x001E1338 -> 0x00153780
[  9] 0x001E1294 -> 0x0014E1F0      [ 51] 0x001E133C -> 0x00153790   <== THE THUNK
```

**So the earlier "40 entries from `0x001E12A0`" was the Session's first-pass window, not the table's true
extent.** **The base is `0x001E1270` with 52 entries, and it has THREE references — which means the
dispatcher can be located rather than inferred.**

**The Session corrects its own first-pass figure here rather than leaving the smaller number standing.**

| Search | Result |
|---|---|
| **rel32 callers of `0x00153790`** | **ZERO** |
| **absolute dword references to `0x00153790`** | **exactly ONE** — file `0x1D13DC` → **`.rdata` VA `0x001E133C`** = **table index 51** |
| **`0x00199DB0`** (the real worker the thunk forwards to) | **ZERO absolute references** |

## Why this materially changes the edge

**The writer trace named *"the third argument to `sub_00153790`"* as the deciding value.** **That framing
implied a CALLER passing an argument.** **But there is no caller — the thunk is INVOKED THROUGH A TABLE.**

> **So the deciding quantity is not an argument at a call site. It is whatever the DISPATCHER supplies as the
> third argument when it selects table entry 39.** **The search must therefore move to the DISPATCHER, not to
> a caller.**

**And `0x00199DB0` having ZERO absolute references while its forwarding thunk has exactly one is itself
informative:** **the thunk exists precisely to adapt the table's calling convention to the worker's.** **That is
what "forwarding thunk" means here — `mov eax,[esp+0x10] / mov ecx,[esp+0xc] / mov edx,[esp+8] / push / push /
push / call` re-orders three stack arguments.**

**So the table's dispatcher pushes three arguments; the thunk reverses their order; the worker takes them.**
**The third argument at the THUNK is the FIRST argument the DISPATCHER pushed.**

## What this does and does NOT establish

**Established:**

1. **`sub_00153790` has zero direct callers and one absolute reference — it is a table entry.**
2. **The table is `0x001E1270..0x001E133C`, 52 entries, non-ascending — a vtable.** *(⚠ CORRECTED: the
   Session's first pass said `0x001E12A0`, 40 entries. That was its WINDOW, not the table. The true base is
   `0x001E1270`, terminated by `0xC3000000` at `0x001E126C`.)*
3. **`0x00153790` is the LAST entry — index 51 of 52.** *(⚠ CORRECTED: first pass said index 39 of 40.)*
4. **`0x00199DB0` has zero absolute references**, so it is reached only through the thunk.
5. **THE VTABLE IS INSTALLED AT `object+0x00`** by `0x00152244 mov dword ptr [esi], 0x1e1270`, on a
   **teardown path** (`dec [esi+0x608]` → `jne` → install). **So `0x001E1270` is an object's vtable, not an
   anonymous dispatch array.**

**NOT established:**

- **Which dispatcher reads this table**, and **what it passes as the arguments.**
- **Whether the table is indexed by a device field, a command code, or something else.**
- **Whether entry 51 is ever selected** — **and if it is not, the `0x00199F45` store never executes.**

> **So the edge is RELOCATED, and the better-posed question is: *where is a virtual call made through
> `object+0x00` at INDEX 51, and what does that caller pass as the worker's ARG1?***
>
> **⚠ The Session's earlier phrasing — *"what dispatcher reads the table at `0x001E12A0`… for entry 39"* —
> used the SUPERSEDED base and index.** **Corrected here.** **And note the argument question is ARG1, not the
> first-of-three: the acceptance correction established that `ebp = ARG1`.**

## The method note, because this is the pattern that keeps paying

**The Session found this by asking a question the packet did not: *"who calls the named edge's parent?"***
**The writer trace correctly identified the deciding VALUE but framed it as a call-site argument.** **One
step of provenance on that framing revealed the thunk is a table entry.**

**Recorded because it is the same move that found the producer: follow the named edge one level up before
writing the successor packet.**

## The shape observation, held at the same arm's length

**The writer trace noted `0x001D5078` is a filename in a table whose base has ZERO references** — **the
signature of type confusion.** **This finding does not change that, and the Session does NOT promote it.**
**A vtable entry being reached with a filename as an argument is CONSISTENT with type confusion, and
consistency is not evidence.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
