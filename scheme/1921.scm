#!./mlanscm -s
!#
; Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
; $Id: 1921.scm,v 1.2 2002/01/30 09:22:12 dustin Exp $

; Temperature conversion for 1921's
(define (ds1921-convert-out x)
  (- (/ x 2.0) 40.0))

; Vars for this thing
(define mlan-conn #f)
(define mlan-rv #f)

; (define device "212C870000402504")
(define device "2183110000C034BB")

(define (mlan-test-dev a)
  (set! mlan-conn (mlan-init a))
	(mlan-search mlan-conn))

(define (mlan-test)
  (mlan-test-dev "/dev/tty00"))

(define (mlan-debug . x)
  (display "!!!SCM!!! ")
  (for-each display x)
  (display " !!!SCM!!!\n"))

(define (mlan-lookup-test)
  (if (not mlan-conn)
	(mlan-test))
  (mlan-debug "Getting block at 16")
  (set! mlan-rv (mlan-getblock mlan-conn device 16 1))
  )

; do my test
(mlan-test)
(mlan-reset mlan-conn)
(mlan-lookup-test)
(display mlan-rv)
(display "\n")
