# Removing the lifted code from history (planned, not run)

**Status: PLANNED, awaiting the owner's explicit go-ahead.** Publishing the rewrite needs a
force-push to `origin`, which `AGENTS.md` forbids without that authorization.

## Decision

Owner, 2026-09-30: the code lifted from the retail XBE leaves the public repository. Done so far:

- The nine hand edits that lived only inside generated files are patches in
  `config/generated-patches.json` (game `8331a6d`; ledger L37, L38).
- `src/recomp/gen/` and `src/recomp/recovered/` are untracked and gitignored (game `52dc97b`);
  `AGENTS.md` "Rebuilding the lifted code" says how to rebuild them from `game/default.xbe`.

Every earlier commit still contains them, here and on GitHub (since `e336a1c`, 2026-09-21). This
record is the plan for removing them from history.

## What the rewrite removes

Every version of every file under `src/recomp/gen/` and `src/recomp/recovered/`, from every
commit. These are the only paths lifted code has ever lived in: over all 609 commits, the files
named `recomp_NNNN.c`, `recomp_dispatch*`, `recomp_funcs.h`, `recomp_stubs*`,
`recomp_abi_deltas*`, `recomp_types.h` and `recovered*.c` appear under no other directory.

Not removed, and not lifted code: `config/recovered-functions.json` and
`config/boundary-fixes.json` (addresses and review prose), `config/xdk-symbols.json` (library
symbol names and addresses), and short disassembly excerpts quoted in `docs/`. Each is derived
from the XBE; whether any should also go is a separate decision.

## Dry run (2026-09-30)

On a throwaway mirror of the local repository at `8331a6d` (before the untracking commit):

```
git clone --mirror <repo> /tmp/pj-scrub.git
cd /tmp/pj-scrub.git
uv run --with git-filter-repo git-filter-repo \
    --path src/recomp/gen --path src/recomp/recovered --invert-paths --force
```

Result: 609 commits kept, no path under either directory left in any commit, pack size
11.59 MiB → 4.73 MiB, and every commit hash changed (the new root is `3230a5e`).

## Consequences

- **Every game commit hash changes.** Records that cite game commits (the plan, the ledger, the
  technical record, review records, the operating history) would point at hashes that no longer
  exist. `git-filter-repo` writes `filter-repo/commit-map` (old → new); rewriting the citations
  from it is part of the job. Toolkit hashes are unaffected.
- **Every clone has to be replaced.** The Windows checkout must copy `src/recomp/` and any other
  ignored local data aside, then re-clone or `git fetch` and `git reset --hard origin/master`.
- **Copies outside this repository remain.** Forks, other clones, and GitHub's cached views and
  pull-request refs keep the old objects; GitHub Support can be asked to purge cached data.
  Anyone who already fetched the repository has the code.

## Steps, once authorized

1. Push everything from both machines; on the Windows host copy `src/recomp/` aside.
2. `git clone --mirror https://github.com/danillogical/poison-jam /tmp/pj-scrub.git`
3. Run the filter above in the mirror (without `--force` on a fresh clone).
4. Verify: `git log --all --name-only --format= | grep -cE '^src/recomp/(gen|recovered)/'` is 0,
   the commit count matches, and the tip's tree matches the current tip apart from those paths.
5. Rewrite game-commit citations in the records from `filter-repo/commit-map`; commit.
6. Force-push `master` (and any tags) to `origin`; record it as
   `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:`.
7. Replace every clone (step 1's copies go back under `src/recomp/`).
8. Optionally ask GitHub Support to purge cached views of the old commits.
