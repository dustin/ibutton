#!./mlanscm -s
!#
; Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
; $Id: 1920.scm,v 1.3 2001/12/11 08:26:07 dustin Exp $
(define ds1920-convert-temperature #x44)
(define ds1920-read-scratchpad #xbe)
(define ds1920-write-scratchpad #x4e)
(define ds1920-copy-scratchpad #x48)
(define ds1920-match-rom #x55)

; Debug printer
(define (mlan-debug . x)
  (display "!!!SCM!!! ")
  (for-each display x)
  (display " !!!SCM!!!\n"))


; Set the high and low values in a 1920
(define (mlan-ds1920-set-values mcon serial low hi)
  (let ((hv hi) (lv low))
	(mlan-access mcon serial)
	(if (<= hv 0)
	  (set! hv (logior #x80 (abs hv))))
	(if (<= lv 0)
	  (set! lv (logior #x80 (abs lv))))
	; Put the data in the scratch pad
	(mlan-debug "Sending to scratchpad")
	(mlan-block mcon #f (list ds1920-write-scratchpad hv lv))
	; match ROM -- not sure why, but this is what I did in C
	(mlan-debug "Matching and copying scratchpad.")
	(mlan-block mcon #t (append
						  (list ds1920-match-rom)
						  (mlan-parse-serial mcon serial)
						  (list ds1920-copy-scratchpad)))
	(mlan-debug "Setting mode to strong5")
	(mlan-setlevel mcon mlan-mode-strong5)
	(mlan-msdelay mcon 500)
	(mlan-debug "Setting mode to normal")
	(mlan-setlevel mcon mlan-mode-normal)))


; Get the raw data from a DS 1920
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
