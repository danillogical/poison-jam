-- A/B two runs on one counter, showing the difference.
--
-- Usage: python -X utf8 scripts/logq.py <runA> <runB> --saved kmem-ab
--
-- Both runs are loaded into one database, keyed by run name, so the comparison
-- is a join rather than a mental subtraction. A counter present in only one run
-- still appears, with NULL on the other side, because a counter that *stopped*
-- being emitted is a finding and not an absence of data.
SELECT coalesce(a.counter, b.counter)          AS counter,
       max(CASE WHEN a.run LIKE '%' THEN a.value END) AS value_a,
       max(CASE WHEN b.run LIKE '%' THEN b.value END) AS value_b,
       max(CASE WHEN b.run LIKE '%' THEN b.value END)
         - max(CASE WHEN a.run LIKE '%' THEN a.value END) AS delta
FROM kmem_summary a
FULL OUTER JOIN kmem_summary b
  ON a.counter = b.counter AND a.run <> b.run
GROUP BY coalesce(a.counter, b.counter)
ORDER BY counter;
