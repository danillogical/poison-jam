-- The present ceiling: the highest present count, when it was first reached, when
-- the last present line was logged, the last rejection diagnostic, and how many
-- distinct (class, method) pairs were admitted as unknown.
SELECT (SELECT max(presents) FROM presents) AS max_presents,
       (SELECT min(t) FROM presents
         WHERE presents = (SELECT max(presents) FROM presents)) AS first_t_at_max,
       (SELECT max(t) FROM presents) AS last_presents_t,
       (SELECT diag FROM pfifo
         WHERE kind IN ('reject', 'still_rejecting')
         ORDER BY lineno DESC LIMIT 1) AS last_reject_diag,
       (SELECT count(DISTINCT (class_id, method)) FROM pfifo
         WHERE kind = 'admit_unknown') AS admitted_unknown;
