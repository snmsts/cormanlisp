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
(win:ExitProcess (if (zerop *problems*) 0 1))
