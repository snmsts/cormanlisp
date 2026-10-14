;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		time.lisp
;;;;	Contents:	Common Lisp time functions.
;;;;	History:	RGC  12/1/96  Created.
;;;;				RGC  3/25/01  Fixed ENCODE-UNIVERSAL-TIME so it now defaults
;;;;							  to the current time zone.
;;;;				RGC 11/23/01  Fixed a problem with handling of local time
;;;;                              offsets and FORMAT-UNIVERSAL-TIME.
;;;;                RGC 9/8/04    DECODE-UNIVERSAL-TIME now handles daylight time
;;;;                              in a manner consistent with other common lisp implementations
;;;;                              (I couldn't understand the standard) and the time zone
;;;;                              does not include the adjustment for daylight time.
;;;;

(in-package :win32)

#! (:export t :library "KERNEL32" :ignore "APIENTRY" :pascal "WINAPI")
typedef struct _LARGE_INTEGER {
		DWORD LowPart;
		LONG HighPart;
} LARGE_INTEGER,*PLARGE_INTEGER;
BOOL WINAPI QueryPerformanceCounter(PLARGE_INTEGER);
BOOL WINAPI QueryPerformanceFrequency(PLARGE_INTEGER);
!#

(in-package :common-lisp)

(defconstant internal-run-time-buffer (ct:malloc (ct:sizeof 'win:LARGE_INTEGER)))
(defconstant internal-time-units-buffer (ct:malloc (ct:sizeof 'win:LARGE_INTEGER)))

(defun large-integer-to-lisp-integer (large-integer)
    (let* ((low  (ct:cref win:LARGE_INTEGER large-integer win::LowPart ))
           (high (ct:cref win:LARGE_INTEGER large-integer win::HighPart)))
        (if (>= high 0)
            (+ low (* high #x100000000))
            (let ((total (+ low (* high #x100000000))))
                (- (+ 1 (lognot total)))))))
            
(defun cl::get-internal-run-time ()
    (os-performance-counter internal-run-time-buffer)
    (large-integer-to-lisp-integer internal-run-time-buffer))

(defun cl::get-internal-time-units-per-second ()
    (os-performance-frequency internal-time-units-buffer)
    (large-integer-to-lisp-integer internal-time-units-buffer))

(defvar calibration-value -1)

;;;; calibrate the timer
(defun calibrate-timer ()
    (let ((tm (get-internal-run-time)) 
  	      (result-time))
	(setq tm (- (get-internal-run-time) tm))
	(setq result-time (/ (float tm) internal-time-units-per-second))
	(setq calibration-value result-time)))
	
(defmacro time (x)
	(let ()
        `(progn (calibrate-timer)
    		(let* ((tm (get-internal-run-time)) 
    			    (gtm cl::*gc-time-counter*)
    			    (result-time)
    			    (result-gc-time)
    			    ret)
    			(setq ret (multiple-value-list ,x))
    			(setq tm (- (get-internal-run-time) tm))
    			(setq gtm (- cl::*gc-time-counter* gtm))
    			(setq result-time (/ (float tm) internal-time-units-per-second))
    			(setq result-gc-time (/ (float gtm) gc-time-units-per-second))
    			(format t "Total Execution time: ~A seconds~%" (max (- result-time calibration-value) 0))
    			(format t "Time spent garbage collecting: ~A seconds~%" result-gc-time)
    			(values-list ret)))))	

(defconstant file-time-1900-01-01 9435484800)

(defun file-time-to-universal-time (ftime)
	(- (truncate ftime 10000000) file-time-1900-01-01))

(defun universal-time-to-file-time (utime)
	(* (+ utime  file-time-1900-01-01) 10000000))
	
;;;
;;; Common Lisp GET-UNIVERSAL-TIME function.
;;;
(defun get-universal-time ()
	(file-time-to-universal-time
		(apply #'system-time-to-file-time (get-system-time))))

;;;
;;;	Returns what local time is at the given universal time:
;;;    how many seconds it is behind universal time, daylight time counted in
;;;    daylight-flag (t if daylight time, nil if standard time)
;;;    time-zone-name (string)
;;;	Where the system cannot tell, local time is universal time.
;;;
(defun local-time-offset (utime)
	(let ((stime (file-time-to-system-time (universal-time-to-file-time utime))))
		(ct:with-fresh-foreign-block (seconds-west :long)
			(ct:with-fresh-foreign-block (daylight :long)
				(ct:with-fresh-foreign-block (name '(:char 128))
					(if (os-time-zone (first stime) (second stime) (fourth stime)
							(fifth stime) (sixth stime) (seventh stime)
							seconds-west daylight name 128)
						(values (ct:cref (:long *) seconds-west 0)
								(/= (ct:cref (:long *) daylight 0) 0)
								(ct:c-string-to-lisp-string name))
						(values 0 nil "Unknown")))))))

;;;
;;;	Common Lisp DECODE-UNIVERSAL-TIME function.
;;;	Usage: (DECODE-UNIVERSAL-TIME universal-time &optional time-zone)
;;;		=> second, minute, hour, date, month, year, day, daylight-p, zone
;;;
(defun decode-universal-time (utime &optional time-zone)
	(let (stime
		  (second 0)
		  (minute 0)
		  (hour 0)
		  (date 0)
		  (month 0)
		  (year 0)
		  (day-of-week 0)
		  (daylight-p nil)
		  (zone time-zone))

		(unless (and (integerp utime) (>= utime 0))
			(error "Invalid universal time: ~A" utime))
		(if zone
			(decf utime (* zone 3600))
			(multiple-value-bind (seconds-west daylight)
				(local-time-offset utime)
				(setq daylight-p daylight)
				(decf utime seconds-west)

				;; The time zone which is returned does not include daylight
				;; time, which is taken to be an hour, as those who look at
				;; DAYLIGHT-P and ZONE take it to be.
				(setq zone (/ (+ seconds-west (if daylight 3600 0)) 3600))))

		(setq stime (file-time-to-system-time 
						(universal-time-to-file-time utime)))
		(setq year (first stime))
		(setq month	(second stime))
		(setq day-of-week (- (third stime) 1))
		(if (= day-of-week -1) (setq day-of-week 6))
		(setq date (fourth stime))
		(setq hour (fifth stime))
		(setq minute (sixth stime))
		(setq second (seventh stime))
		(values second minute hour date month year day-of-week daylight-p zone)))

;;;
;;;	Common Lisp ENCODE-UNIVERSAL-TIME function.
;;;	Usage: (ENCODE-UNIVERSAL-TIME second minute hour date month year
;;;				&optional time-zone)
;;;	This varies from the standard slightly, in that two digit years
;;; are relative to the year 2000 rather than the current year.
;;;
(defun encode-universal-time (second minute hour date month year
			&optional time-zone)
	(if (<= 0 year 99)
		(if (>= year 50) 
			(setq year (+ 1900 year))
			(setq year (+ 2000 year))))		
	(let ((utime (file-time-to-universal-time
			(system-time-to-file-time year month 0 
				date hour minute second 0))))
		(if time-zone
			(+ utime (* 3600 time-zone))

			;; How far local time was behind then is not known before the
			;; universal time is: the first answer gets near enough to it
			;; for the second one to be that of the right side of a change
			;; to or from daylight time.
			(+ utime (local-time-offset (+ utime (local-time-offset utime)))))))

;;;
;;;	Common Lisp GET-DECODED-TIME function.
;;;
(defun get-decoded-time ()
	(decode-universal-time (get-universal-time)))

(defconstant month-names #("January" "February" "March" "April" "May"
	"June" "July" "August" "September" "October" "November" "December"))

;;;
;;;	Corman Lisp FORMAT-UNIVERSAL-TIME function.
;;;
(defun format-universal-time (time &optional (stream *standard-output*))
	(multiple-value-bind (second minute hour date
					month year day-of-week
					daylight-p time-zone)
		(decode-universal-time time)
		(declare (ignore second day-of-week daylight-p time-zone))
		(format stream "~2,'0D:~2,'0D ~A ~A ~A, ~A ~A"
			(if (= hour 0) 12 (if (> hour 12) (- hour 12) hour))
			minute
			(if (= hour 0) "AM" (if (>= hour 12) "PM" "AM"))
			(aref month-names (- month 1))
			date
			year
			(nth-value 2 (local-time-offset time)))))

;;;
;;;	Returns three values, for the time it is now: 
;;;    the bias (in hours), daylight time counted in
;;;    daylight-flag (t if daylight time, nil if standard time)
;;;    time-zone-name (string)
;;;                    
(defun cl::local-time-zone ()
	(multiple-value-bind (seconds-west daylight-p name)
		(local-time-offset (get-universal-time))
		(values (/ seconds-west 3600) daylight-p name)))


;;;
;;; Common Lisp GET-INTERNAL-REAL-TIME function.
;;;
(defun get-internal-real-time () (get-internal-run-time))

