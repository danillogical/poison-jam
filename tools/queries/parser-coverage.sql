-- Lines each tag-specific parser accounted for, against the total.
--
-- This is the coverage witness for every other query in this directory. A
-- parser that stops matching after a log-format change produces a SMALLER number
-- that reads like a real result -- measured: the `[TRACE]` parser matched zero of
-- 1268 lines because the direction token is `->` and the pattern expected a
-- single character. The count alone did not reveal it; the comparison with the
-- tag histogram did.
SELECT tag,
       count(*) AS lines
FROM lines
WHERE tag <> ''
GROUP BY tag
ORDER BY lines DESC;
