-- Kernel calls by ordinal, most frequent first.
--
-- Ordinal 277 (RtlEnterCriticalSection) and 294 (RtlLeaveCriticalSection) lead by
-- a wide margin on the boot path, which is what makes the thunk table the
-- terminal event's location: those two are called through slots 65 and 66 of the
-- 120-slot table at 0x001C3F60.
SELECT ordinal,
       count(*)                AS calls,
       min(lineno)             AS first_line,
       count(DISTINCT tid)     AS threads,
       min(slot)               AS slot
FROM kernel_calls
GROUP BY ordinal
ORDER BY calls DESC;
