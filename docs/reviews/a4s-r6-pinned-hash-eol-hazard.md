# A4s-r6: the pinned-hash / line-ending hazard (measured, awaiting an interpretation ruling)

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Status:** **open interpretation question** sent to the persistent Advisor. Recorded now so the
measurement cannot be lost while the ruling is pending. **Not** a packet revision — §5.4 governs that,
and this is a question about how to *read* a frozen criterion.

## The measurement

`A4s-r6` pins its two gating tools by SHA-256 and states: *"The scanner's bytes are verified against the
pinned SHA-256 before each use (P0.10); any byte change reopens `AC-STRUCT` (§2.4.7)."*

The game repo has **`core.autocrlf=true` and no `.gitattributes`**. What a fresh checkout writes was
measured with `git checkout-index -a --prefix=<temp>` (safe: it writes to a temp directory and does not
touch the repository):

| Artifact | Staged blob (what git stores) | Fresh checkout (what a working tree gets) |
|---|---|---|
| `scripts/check-merge-structure.py` | 14494 bytes, CRLF **0**, LF 355 → `A1FDCE26755DAB3A0B6AA6E23D380A8F95E097439C2BE91D6318BF4402AA1AFF` **= the pin** | 14849 bytes, CRLF **355** → `D572DB08A005A0A68D470B93EAED3408AC8D226DB6A6157153BF81A7DD642211` **≠ the pin** |
| `tests/test_merge_structure.py` | 8495 bytes, CRLF **0** → `A42F4A1E2C473892DC28A0F5B21923F4BC0AEFCFFE859C7FC20006F49DB0744C` **= the pin** | 8744 bytes, CRLF **249** → `23B5DDEE129F2D311665E395D1CC7AA0F63B23D05648B71FD5B6F44B870ED5E` **≠ the pin** |

`git ls-files --eol` reports `i/lf w/lf attr/` (empty attribute) for every one of these files, so **no
attributes are in force**. Existing tracked files such as `scripts/build-jsrf.py` are `w/lf` today only
because they have not been re-checked-out on this machine; the same conversion applies to them.

## Why this matters

The pin is a hash of the **LF (git blob) bytes**. Any fresh checkout converts these paths to CRLF in the
working tree, so a literal executor that hashes the **working-tree file** would get a **different** hash
and fail `AC-STRUCT`'s identity check **on a correct tree** — a false FAIL caused by a checkout property
rather than by any change to the artifact.

## The question put to the Advisor

Which bytes does the criterion mean?

- **Session reading (offered for confirmation):** the pin is an identity check on the **tracked content**,
  so the executor hashes the blob as git stores it — `git show :<path>` / `git hash-object` /
  `git cat-file blob :<path>` — **not** the raw working-tree file. Hashing the working tree would make the
  pin depend on `core.autocrlf`, which is a property of the checkout, not of the artifact.
- **Also asked:** whether the same reading applies to the existing P0.10 byte-verification language
  elsewhere in the packet, or is `AC-STRUCT`-specific.

## What was deliberately NOT done

- **No `.gitattributes` was added.** Forcing LF for these paths would change checkout behaviour repo-wide
  for every existing file, which is not a change to make unilaterally, and it would be a repo-wide
  normalization of exactly the kind the owner's startup directive forbids.
- **No packet edit.** `A4s-r6` is frozen at `75207C41…86E3`; §5.4 permits revision only on a §3.1
  blocking finding, a `PREMISE_CHANGED`, an Advisor policy ruling, or an owner change. This is an
  interpretation question, which §5.4 says is *"settled by an Advisor interpretation ruling that
  execution follows, not by a revision."*
- **No `git add --renormalize`**, and nothing was committed that would alter stored line endings.

## Practical note for the executor

Until the ruling arrives, the safe form is to verify against the blob:

```powershell
# LF (tracked) bytes — matches the pin
git -C C:\Users\logic\Repos\my_xbox_game show :scripts/check-merge-structure.py | Out-Null  # inspect via python
```

or, equivalently in Python:

```python
import hashlib, subprocess
b = subprocess.run(['git', 'show', ':scripts/check-merge-structure.py'],
                   capture_output=True, cwd=ROOT).stdout
hashlib.sha256(b).hexdigest().upper()   # A1FDCE26…AFF
```
