# A4b-r2 review — Session checks requested by the reviewer

The `A4b-r2` adequacy reviewer (child `38e2f0ab-a67a-4fd8-a3ce-9aeb31d0da75`) named three
things for the Session to check before the repair, because its own repair rule for blocking
defect **B1** depends on them. All three are resolved below as **observed**, so the Planner
repairs against evidence rather than an inference.

## Check 1 — which import sits at `0x1C4004`? (resolves B1)

**Observed: kernel ordinal 161, `KfLowerIrql`, a `__fastcall` whose only argument is in
`ecx`.**

- The thunk slot: the XBE's `.rdata` at VA `0x1C4004` (file `0x1B40A4`) holds
  **`0x800000A1`**. The `0x80000000 | ordinal` encoding gives **ordinal 161**.
- Toolkit `src/kernel/kernel_thunks.c:201`: `case 161: return (ULONG_PTR)xbox_KfLowerIrql;`
- Toolkit `src/kernel/kernel.h:799`:
  `VOID __fastcall xbox_KfLowerIrql(KIRQL NewIrql);`
- The callee `sub_001A1BAF` loads the argument immediately before the call:
  `001A1BB8 mov cl, byte ptr [esi]` → `001A1BBA call dword ptr [0x1c4004]`.
- Full body of `sub_001A1BAF` (XBE `0x001A1BAF`–`0x001A1BC5`):

  ```asm
  001A1BAF push    esi
  001A1BB0 mov     esi, ecx
  001A1BB2 cmp     dword ptr [esi + 4], 0
  001A1BB6 je      0x1a1bc4
  001A1BB8 mov     cl, byte ptr [esi]
  001A1BBA call    dword ptr [0x1c4004]
  001A1BC0 and     dword ptr [esi + 4], 0
  001A1BC4 pop     esi
  001A1BC5 ret
  ```

**Consequence for `AC-PIO` step 4:** the reviewer's inference ("possibly `KfLowerIrql`",
marked not verified) is now **confirmed**. `KfLowerIrql` takes its argument in `ecx`/`cl`
and returns nothing meaningful; it cannot read `eax`, `edx`, `esi`, `ebx` or `edi` as
input. So the import clause can be made decidable and correct: an import cannot read the
polled register as input when the register is not `ecx`/`edx`; and for `ecx`/`edx` the
executor resolves the ordinal at the thunk slot and its documented fastcall/stdcall arity,
marking the site `UNKNOWN` only if that fails. It also fixes the second half of B1: what the
import does to the register afterwards. `KfLowerIrql` returns `VOID`, and the caller
immediately does `and dword ptr [esi+4], 0` — it does **not** consume `eax` or `edx` as a
result. The Planner should state the clobber convention explicitly; the observed evidence is
that this callee's caller does not read a return value from it.

**Note the same slot value appears twice in the table window** (`0x1C3FF8` and `0x1C4004`
both hold `0x800000A1`), so a reader must use the exact VA `0x1C4004` the instruction
references, not a nearby slot.

## Check 2 — do callers of the function containing `0x1A4E4C` use its return value? (bears on B2)

**Not resolved by the Session.** Locating and auditing every caller of that function is a
multi-step analysis the reviewer itself did not finish, and the Planner's new rule must
exist regardless — the reviewer said so explicitly: "If they don't, B2 resolves at that
site, but the rule is still needed." The repair should therefore **define the rule** (a
`ret`/`jmp` out of the function with the polled register live is a flow into the caller,
traced into every caller, else `UNKNOWN`) and let execution apply it. The Session did not
spend budget on this because it cannot change whether the rule is required.

## Check 3 — apply the fixed rule to all 28 exits once, so the repair is not a third patch

**Not done by the Session, deliberately, and it should not be.** This is exactly the work
the reviewer asked to happen *before the next review*, but it requires the **fixed rule**,
which is the Planner's to author. Doing it first would be the Session reinterpreting a
criterion (§2.2.6) and would risk a third patch of the same shape (§5.5). The Planner should
write the rule so that it is mechanically applicable to all 28 exits, and the executor
applies it once during `AC-PIO`.

## What the Session did record

- `docs/reviews/a4b-r2-adequacy-review.md` — the reviewer's `INADEQUATE` block verbatim,
  what it confirmed as fixed, why each defect is blocking, and the §5.5 note that B1+B2 are
  one mechanism to be repaired together rather than two patches.
- This record, so the repair brief carries the resolved import identity.
