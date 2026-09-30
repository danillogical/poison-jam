# Citation store: tools/citations/records/citations.jsonl

Each line is one `jsrf-citation/1` record written by `scripts/cite.py record`. The store is
a **cited output**, not a claim, so `cite.py check` does not lint its own literals; a record
that cites the store names it with `CITE-STORE:`.

A citation is keyed by **the property the decision classifies** (`docs/agent-workflow.md`
section 6.1.6b), not by the read that observed it. `record --key-kind guest-va` keys by
guest virtual address; `--key-universe FILE` bounds the store by the finite key set the
owning packet enumerates from source, and `record` refuses a key outside it.

Records here:

- `guest-va 0x00193D96` — the T10 transposition control. `0x00193D96` is the instruction
  start of `mov esi, ecx` at the head of `sub_00193D90`; the historical error recorded
  `0x00193D62`, which is inside the instruction at `0x00193D61` and is never an instruction
  start. The command is re-runnable, so the value is re-derivable rather than trusted.
