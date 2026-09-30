-- The `[KMEM] summary` line as one row per counter, so two runs can be compared
-- without reading two long lines side by side.
--
-- `docs/jsrf-run-profiles.md` makes several of these counters decision inputs for
-- milestone 09 (`commit_rejected`, `release_failed`, `region_table_full`), and a
-- comparison done by eye is a transcription (plan W5) waiting to be wrong.
SELECT counter, value
FROM kmem_summary
ORDER BY counter;
