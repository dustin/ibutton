; (define (l) (load "alldev.scm"))
; Copyright (c) 2001  Dustin Sallings <dustin@spy.net>
; $Id: alldev.scm,v 1.2 2001/12/10 08:23:16 dustin Exp $
; arch-tag: FD6D3437-13E5-11D8-AB58-000393CFE6B8
(load "1920.scm")

(define ds-serial-table '(
					   (#x01 . "DS1990 Serial Number iButton")
					   (#x02 . "DS1991 Multi-key iButton")
					   (#x04 . "DS1992/1993/1994 Memory iButton")
					   (#x09 . "DS9097u 1-wire to RS232 converter")
					   (#x0a . "DS1995 64kbit Memory iButton")
					   (#x0b . "DS1985 16kbit Add-only iButton")
					   (#x0c . "DS1986 64kbit Memory iButton")
					   (#x0f . "DS1986 64kbit Add-only iButton")
					   (#x10 . "DS1820/1920 Temperature sensor")
					   (#x12 . "DS2407 2 channel switch")
					   (#x14 . "DS1971 256bit EEPROM iButton")
					   (#x18 . "DS1962/1963 Monetary iButton")
					   (#x1a . "DS1963L Monetary iButton")
					   (#x21 . "DS1921 Thermochron")
					   (#x26 . "DS2438 Battery Monitor/Humidity Sensor")
					   (#x23 . "DS1973 4kbit EEPROM iButton")
					   (#x96 . "DS199550-400 Java Button")))

; This is the command registry which maps serial IDs to commands to get the
; data.
(define mlan-cmd-registry '(
							(#x10 . get-ds1920-data)
							(#x09 . print-name)))

; Get the type identifier from the serial number
(define (get-type x)
  (string->number (substring x 0 2) 16))

; Get the type name (from serial-table) for the given serial number
(define (get-type-name x)
  (let ((l (assq (get-type x) ds-serial-table)))
	(if l
	  (cdr l)
	  (throw 'unknowndev "Unknown device"))))

; This will abstractly get the MLAN data for a given serial on a given
; connection.
(define (get-mlan-data mcon ser)
  (let ((c (assq (get-type ser) mlan-cmd-registry)))
	(if c
	  ((eval (cdr c)) mcon ser)
	  (throw 'nocommand (string-append "No registered command for " ser)))))

; This does the same as above, but in the case of missing commands, it will
; simply return an entry 
(define (safe-get-mlan-data mcon ser)
  (catch
	'nocommand
	(lambda ()
	  (get-mlan-data mcon ser))
	(lambda (k . a)
	  (list 'error (car a)))))

;
; ------------------------- Begin specific commands ------------------------
;

; Just print the name
(define (print-name mcon ser)
  (list 'name ser (get-type-name ser)))

; This is the command to read a 1920 (of course, the 1920 lib has to be
; loaded first.
(define (get-ds1920-data mcon ser)
  (list 'ds1920 ser
		(mlan-ds1920-decode-data
		  (mlan-ds1920-data mcon ser))))

