-- The terminal event: invalid indirect calls, and whether one preceded the
-- guest's own checkpoints.
--
-- `docs/reviews/strict-horizon-ledger.md` records that the first failing SITE
-- varies between runs while the EVENT (the thunk table being overwritten) does
-- not. This query is the per-run form of that: which return address faulted, in
-- what order, and how far the guest had got.
SELECT icalls.lineno,
       icalls.target,
       icalls.ret       AS return_address,
       icalls.tid,
       (SELECT max(name) FROM checkpoints
         WHERE checkpoints.lineno < icalls.lineno) AS last_checkpoint
FROM icalls
ORDER BY icalls.lineno;
