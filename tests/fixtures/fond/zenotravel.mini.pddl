; One aircraft, one person, three cities, three fuel levels, for the
; zenotravel domain of the FOND4LTLf corpus (whitemech/FOND4LTLf,
; tests/data/pddl-domains/zenotravel/domain.pddl).  The corpus's own p01 has
; more than 20000 reachable states; this keeps the encoding checkable.
(define (problem zeno_mini)
  (:domain zenotravel)
  (:objects c0 c1 c2 - city p0 - person a0 - aircraft f0 f1 f2 - flevel)
  (:init (next f0 f1) (next f1 f2)
         (atperson p0 c0) (not-boarding p0) (not-debarking p0)
         (at-aircraft a0 c0) (fuel-level a0 f2) (not-refueling a0))
  (:goal (atperson p0 c2))
)
