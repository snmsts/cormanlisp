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

;;; C calls back into Lisp, which allocates enough there for the
;;; garbage collector to run meanwhile.
(ct:defun-dll c-qsort ((base (:void *)) (count :unsigned-long) (size :unsigned-long)
		(compare (:void *)))
	:return-type :void
	:library-name #+os-windows "msvcrt.dll" #-os-windows "libc.so.6"
	:entry-name "qsort"
	:linkage-type :c)
(defvar *compared* nil)
(ct:defun-c-callback compare-longs ((a (:long *)) (b (:long *)))
	(let ((x (ct:cref (:long *) a 0)) (y (ct:cref (:long *) b 0)))
		(push (make-list 50 :initial-element (cons x y)) *compared*)
		(when (> (length *compared*) 200) (setq *compared* nil))
		(cond ((< x y) -1) ((> x y) 1) (t 0))))
(check "callbacks from C"
	(let* ((count 20000) (numbers (ct:malloc (* 4 count))))
		(dotimes (i count) (setf (ct:cref (:long *) numbers i) (mod (* i 7919) 10007)))
		(c-qsort numbers count 4 (ct:get-callback-procinst 'compare-longs))
		(prog1
			(dotimes (i (- count 1) t)
				(when (> (ct:cref (:long *) numbers i) (ct:cref (:long *) numbers (+ i 1)))
					(return nil)))
			(ct:free numbers))))

;;; Files are copied, renamed and mapped into memory by the kernel, and
;;; COMPILE-FILE reads its input as a mapped file.
(with-open-file (s "test-copy-1.tmp" :direction :output :if-exists :supersede)
	(dotimes (i 5000) (format s "line ~D~%" i)))
(check "copying, renaming and mapping a file"
	(and (ccl::copy-file "test-copy-1.tmp" "test-copy-2.tmp")
		(ccl::copy-file "test-copy-1.tmp" "test-copy-2.tmp")
		(not (ccl::copy-file "test-copy-0.tmp" "test-copy-2.tmp"))
		(ccl::move-file "test-copy-2.tmp" "test-copy-3.tmp")
		(not (probe-file "test-copy-2.tmp"))
		(multiple-value-bind (address length)
			(ccl:map-file "test-copy-3.tmp")
			(and address
				(= length (with-open-file (s "test-copy-1.tmp") (file-length s)))
				(= (ct:cref (:unsigned-char *) address 0) 108)	; the letter l
				(ccl:unmap-file address)))
		(not (ccl:map-file "test-copy-0.tmp"))))
(with-open-file (s "test-compile.tmp.lisp" :direction :output :if-exists :supersede)
	(format s "(defun compiled-in-test (x) (* x 3))~%(defvar *compiled-in-test* (compiled-in-test 14))~%"))
(check "COMPILE-FILE and loading what it wrote"
	(let ((output (compile-file "test-compile.tmp.lisp")))
		(and output
			(load output)
			(eql (symbol-value '*compiled-in-test*) 42)
			(delete-file output))))
(dolist (name '("test-copy-1.tmp" "test-copy-3.tmp" "test-compile.tmp.lisp"))
	(when (probe-file name) (delete-file name)))

;;; A connection over the loopback interface, and a datagram.
(defvar *socket-server* nil)
(th:create-thread
	(lambda ()
		(setq *socket-server*
			(handler-case
				(sockets:with-server-socket (s :host "127.0.0.1" :port 18431)
					(setq *socket-server* :listening)
					(let ((rs (sockets:accept-socket s)))
						(sockets:write-socket-line rs
							(format nil "echo: ~A" (sockets:read-socket-line rs)))
						(sockets:close-socket rs)
						:done))
				(error (c) c)))))
(dotimes (i 50) (when *socket-server* (return)) (sleep 0.1))
(check "sockets: a connection"
	(and (equal (ignore-errors
					(sockets:with-client-socket (c :host "127.0.0.1" :port 18431)
						(sockets:write-socket-line c "hello")
						(sockets:read-socket-line c)))
				"echo: hello")
		(dotimes (i 50) (when (eq *socket-server* :done) (return t)) (sleep 0.1))))
;;; The threads end before a check is written, so that what is written
;;; when a thread ends does not come in the middle of it.
(defvar *datagram-server* nil)
(th:create-thread
	(lambda ()
		(ignore-errors
			(sockets:with-server-socket (s :host "127.0.0.1" :port 18432 :type :datagram)
				(multiple-value-bind (string address) (sockets:receive-from s)
					(sockets:send-to s (format nil "got ~A" string) :to address))))
		(setq *datagram-server* :done)))
(sleep 1)
(let ((answer (ignore-errors
				(sockets:with-client-socket (c :host "127.0.0.1" :port 18432 :type :datagram)
					(sockets:send-to c "ping")
					(sockets:receive-from c)))))
	(dotimes (i 50) (when *datagram-server* (return)) (sleep 0.1))
	(sleep 0.5)
	(check "sockets: a datagram" (equal answer "got ping")))

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
