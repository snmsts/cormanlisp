;;;;
;;;;	File:       run-tests.lisp
;;;;   Contents:   Runs the tests in this directory and exits.
;;;;
;;;;	clconsole -execute test\run-tests.lisp
;;;;
;;;;	The exit code is 0 if nothing got worse than what is recorded below.
;;;;

(in-package "COMMON-LISP-USER")

;;; Examples from the HyperSpec which are known to fail, and how many
;;; pass. The run through them stops in chapter 8 at a #S(TRUCK ...)
;;; which is read before the DEFSTRUCT above it has been evaluated.
(defconstant known-ansi-failures 20)
(defconstant expected-ansi-passes 848)

(defvar *problems* 0)

(defun check (name ok)
	(unless ok (incf *problems*))
	(format t "~&### ~A: ~A~%" name (if ok "ok" "FAILED")))

;;; REMHASH of a key which is not there used to loop forever when
;;; another key was in the same bucket.
(let ((h (make-hash-table)))
	(dotimes (i 200) (setf (gethash i h) i))
	(check "REMHASH, keys not in the table"
		(and (dotimes (i 200 t) (when (remhash (+ i 1000) h) (return nil)))
			(= (hash-table-count h) 200)))
	(check "REMHASH, keys in the table"
		(and (dotimes (i 200 t) (unless (remhash i h) (return nil)))
			(= (hash-table-count h) 0)))
	(check "REMHASH, NIL in an empty table"
		(and (not (remhash nil h)) (= (hash-table-count h) 0))))

;;; A float which a foreign function returns, or which an arithmetic
;;; function has just computed, used to get lost when the node for it
;;; was allocated during a garbage collection.
(check "floats returned by foreign functions"
	(dotimes (i 100000 t) (unless (eql (read-from-string "1.2") 1.2) (return nil))))
(check "floats computed during a garbage collection"
	(let ((x 1.5d0)) (dotimes (i 600000 t) (unless (= (+ x 1d0) 2.5d0) (return nil)))))

;;; Local time used to be universal time where the clock is never
;;; changed, an hour behind while daylight saving time was in effect,
;;; and in a zone of whole hours.
(check "DECODE-UNIVERSAL-TIME, a given time zone"
	(and (equal (multiple-value-list (decode-universal-time 0 0))
				'(0 0 0 1 1 1900 0 nil 0))
		(equal (multiple-value-list (decode-universal-time 3900000000 0))
				'(0 20 21 2 8 2023 2 nil 0))
		(equal (multiple-value-list (decode-universal-time 3900000000 -11/2))
				'(0 50 2 3 8 2023 3 nil -11/2))
		(equal (multiple-value-list (decode-universal-time 3900000000 8))
				'(0 20 13 2 8 2023 2 nil 8))))
(check "ENCODE-UNIVERSAL-TIME, a given time zone"
	(and (= (encode-universal-time 0 0 0 1 1 1900 0) 0)
		(= (encode-universal-time 0 20 21 2 8 2023 0) 3900000000)
		(= (encode-universal-time 0 50 2 3 8 2023 -11/2) 3900000000)
		(= (encode-universal-time 0 20 13 2 8 2023 8) 3900000000)))
(check "local time is what the system says it is"
	(let ((system (cl::get-local-time)))
		(multiple-value-bind (second minute hour date month year)
			(get-decoded-time)
			(< (abs (- (encode-universal-time second minute hour date month year 0)
						(encode-universal-time (seventh system) (sixth system)
							(fifth system) (fourth system) (second system)
							(first system) 0)))
				60))))
(check "local time, there and back"
	(dotimes (i 24 t)
		(let ((utime (encode-universal-time 0 30 12 15 (+ (mod i 12) 1) (+ 2024 (floor i 12)))))
			(multiple-value-bind (second minute hour date month year)
				(decode-universal-time utime)
				(unless (and (= second 0) (= minute 30) (= hour 12) (= date 15)
							(= month (+ (mod i 12) 1)) (= year (+ 2024 (floor i 12))))
					(return nil))))))
(check "local time, the zone and daylight saving time"
	(dotimes (i 24 t)
		(let ((utime (encode-universal-time 0 30 12 15 (+ (mod i 12) 1) (+ 2024 (floor i 12)) 0)))
			(multiple-value-bind (second minute hour date month year day daylight-p zone)
				(decode-universal-time utime)
				(unless (equal (list second minute hour date month year day nil)
							(butlast (multiple-value-list
								(decode-universal-time utime (if daylight-p (- zone 1) zone)))))
					(return nil))))))

;;; Running out of stack is a condition, and the stack can be run out
;;; of again once its end has been protected again.
(defun endless (n) (+ 1 (endless (+ n 1))))
(check "running out of stack"
	(dotimes (i 3 t)
		(unless (handler-case (endless 0)
					(storage-condition () (cl::protect-stack)))
			(return nil))))

(load "test/testkit.lisp")
(load "test/test-sequences.lisp")
(check "sequences" (zerop (test-sequences-module)))

(handler-case (load "test/ansi-examples.lisp")
	(error (condition)
		(format t "~&### ANSI examples stopped: ~A~%" condition)))
(format t "~&### ANSI examples: ~D passed, ~D failed~%" *passed-count* *failed-count*)
(check "ANSI examples"
	(and (>= *passed-count* expected-ansi-passes)
		(<= *failed-count* known-ansi-failures)))

(format t "~&### ~:[~D problem(s)~;All tests passed~]~%" (zerop *problems*) *problems*)
(force-output)
(cl::os-exit-process (if (zerop *problems*) 0 1))
