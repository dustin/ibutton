#!./mlanscm -s
!#
; Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
; $Id: 1920.scm,v 1.1 2001/12/10 08:11:19 dustin Exp $
(define ds1920-convert-temperature #x44)
(define ds1920-read-scratchpad #xbe)

(define (mlan-debug . x)
  (display "!!!SCM!!! ")
  (for-each display x)
  (display " !!!SCM!!!\n"))

(define (mlan-ds1920-data mcon serial)
  (mlan-access mcon serial)
  (mlan-touchbyte mcon ds1920-convert-temperature)
  (mlan-setlevel mcon mlan-mode-strong5)
  (mlan-msdelay mcon 500)
  (mlan-setlevel mcon mlan-mode-normal)
  (mlan-access mcon serial)
  (mlan-block mcon #f
  	(list ds1920-read-scratchpad #xff #xff #xff #xff #xff #xff #xff #xff #xff)))

; Get the 1920 data from the given list
; Returns the current reading, the set low, and the set high
(define (mlan-ds1920-decode-data l)
  (let ((hi (logand (list-ref l 3) #x7f))
		(low (logand (list-ref l 4) #x7f))
		(temp 0) (tsht (list-ref l 1))
		(cr (list-ref l 7)) (cpc (list-ref l 8)))
	; Invert the lows and highs of the flags say so
	(if (not (= 0 (logand #x80 (list-ref l 3))))
		(set! hi (- 0 hi)))
	(if (not (= 0 (logand #x80 (list-ref l 4))))
		(set! low (- 0 low)))
	; Get the rough reading
	(if (not (= 0 (logand #x01 (list-ref l 2))))
	  (set! tsht (logior tsht #xffffff00)))
	(set! temp (- (/ tsht 2.0) 0.25))
	; OK, now use the CR and CPC fields to get a more accurate reading
	(if (= 0 cr)
	  (error "CR was 0"))
	(set! temp (+ (/ (- cpc cr) cpc) temp))
	(list temp low hi)))

; do my test
(define (mytest)
  (display
	(mlan-ds1920-decode-data
	  (mlan-ds1920-data
	  	(mlan-init "/dev/tty00")
	  	"1001BB2A00000098")))
  (display "\n"))
