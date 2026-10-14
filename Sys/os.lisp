;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		os.lisp
;;;;	Contents:	What Lisp asks of the operating system, by way of the
;;;;				kernel, which knows how each operating system is asked.
;;;;

(in-package :common-lisp)

;;;
;;;	Files. One is known by a number, OS-NO-FILE if it could not be opened.
;;;
(defconstant os-no-file -1)

;;; what a file is opened for: either, both, or 0 to find out whether it can be
(defconstant os-read-access 1)
(defconstant os-write-access 2)

;;; what opening does about the file being there or not
(defconstant os-open-existing 0)		; fails if there is no such file
(defconstant os-create-always 1)		; an empty file in place of any there was
(defconstant os-open-always 2)			; the file there is, or else a new one

(ct:defun-kernel os-open-file ((path (:unsigned-char *)) (access :long) (mode :long))
   :return-type :long
   :kernel-name cl::%os-open-file
   :linkage-type :c)

(ct:defun-kernel os-close-file ((file :long))
   :return-type :long-bool
   :kernel-name cl::%os-close-file
   :linkage-type :c)

;;; At the end of the file this succeeds with nothing read.
(ct:defun-kernel os-read-file ((file :long) (buffer (:void *)) (size :unsigned-long)
		(size-read (:unsigned-long *)))
   :return-type :long-bool
   :kernel-name cl::%os-read-file
   :linkage-type :c)

(ct:defun-kernel os-write-file ((file :long) (buffer (:void *)) (size :unsigned-long)
		(size-written (:unsigned-long *)))
   :return-type :long-bool
   :kernel-name cl::%os-write-file
   :linkage-type :c)

(ct:defun-kernel os-file-size ((file :long))
   :return-type :unsigned-long
   :kernel-name cl::%os-file-size
   :linkage-type :c)

;;; The next read or write is OFFSET bytes from the start of the file, or
;;; from its end if FROM-END is not 0. Returns where that is, -1 on failure.
(ct:defun-kernel os-seek-file ((file :long) (offset :long) (from-end :long))
   :return-type :long
   :kernel-name cl::%os-seek-file
   :linkage-type :c)

(ct:defun-kernel os-delete-file ((path (:unsigned-char *)))
   :return-type :long-bool
   :kernel-name cl::%os-delete-file
   :linkage-type :c)

;;; When a file was created, last read and last written. Each is stored
;;; where the argument points, as a 64-bit count of the tenths of a
;;; microsecond since the start of 1601, universal time.
(ct:defun-kernel os-file-times ((file :long) (created (:void *)) (accessed (:void *))
		(written (:void *)))
   :return-type :long-bool
   :kernel-name cl::%os-file-times
   :linkage-type :c)

(ct:defun-kernel os-set-file-times ((file :long) (created (:void *)) (accessed (:void *))
		(written (:void *)))
   :return-type :long-bool
   :kernel-name cl::%os-set-file-times
   :linkage-type :c)

;;; What a file is, as bits: the ones Windows has, of which only read-only
;;; (#x01) and directory (#x10) are known on every system. Returns
;;; #xffffffff if there is no such file or directory.
(ct:defun-kernel os-file-attributes ((path (:unsigned-char *)))
   :return-type :unsigned-long
   :kernel-name cl::%os-file-attributes
   :linkage-type :c)

(ct:defun-kernel os-set-file-attributes ((path (:unsigned-char *)) (attributes :unsigned-long))
   :return-type :long-bool
   :kernel-name cl::%os-set-file-attributes
   :linkage-type :c)

;;; The path in full, from the root on down, whether or not there is such a
;;; file. Returns the length of the full path, 0 on failure.
(ct:defun-kernel os-full-path ((path (:unsigned-char *)) (full-path (:unsigned-char *))
		(size :unsigned-long))
   :return-type :unsigned-long
   :kernel-name cl::%os-full-path
   :linkage-type :c)

;;;
;;;	Directories
;;;

;;; what goes between a directory and what is in it
(defconstant os-directory-separator #+os-windows "\\" #-os-windows "/")

(ct:defun-kernel os-current-directory ((path (:unsigned-char *)) (size :unsigned-long))
   :return-type :long-bool
   :kernel-name cl::%os-current-directory
   :linkage-type :c)

(ct:defun-kernel os-set-current-directory ((path (:char *)))
   :return-type :long-bool
   :kernel-name cl::%os-set-current-directory
   :linkage-type :c)

(ct:defun-kernel os-create-directory ((path (:unsigned-char *)))
   :return-type :long-bool
   :kernel-name cl::%os-create-directory
   :linkage-type :c)

;;; Gives a file or a directory another name, which may be in another directory.
(ct:defun-kernel os-move-file ((from (:unsigned-char *)) (to (:unsigned-char *)))
   :return-type :long-bool
   :kernel-name cl::%os-move-file
   :linkage-type :c)

;;;
;;;	Looking through a directory for the names which match a pattern. The
;;;	pattern is a path, in the last part of which * stands for any characters
;;;	and ? for any one.
;;;
(ct:defcstruct os-found-file
	((is-directory :unsigned-long)
	 (size :unsigned-long)
	 (name (:char 260))))

;;; Returns 0 if nothing matches. Otherwise the first match is in FOUND, and
;;; the search is to be closed when it is done with.
(ct:defun-kernel os-find-first-file ((pattern (:char *)) (found (os-found-file *)))
   :return-type :long
   :kernel-name cl::%os-find-first-file
   :linkage-type :c)

;;; Returns NIL when there are no more.
(ct:defun-kernel os-find-next-file ((search :long) (found (os-found-file *)))
   :return-type :long-bool
   :kernel-name cl::%os-find-next-file
   :linkage-type :c)

(ct:defun-kernel os-find-close ((search :long))
   :return-type :void
   :kernel-name cl::%os-find-close
   :linkage-type :c)

;;;
;;;	Time
;;;

(ct:defun-kernel os-sleep ((milliseconds :unsigned-long))
   :return-type :void
   :kernel-name cl::%os-sleep
   :linkage-type :c)

;;; A counter which goes up steadily, and by how much each second. Each is
;;; stored where the argument points, as a 64-bit integer.
(ct:defun-kernel os-performance-counter ((count (:void *)))
   :return-type :void
   :kernel-name cl::%os-performance-counter
   :linkage-type :c)

(ct:defun-kernel os-performance-frequency ((frequency (:void *)))
   :return-type :void
   :kernel-name cl::%os-performance-frequency
   :linkage-type :c)

;;; What local time is at the universal time with the given parts: how many
;;; seconds it is behind universal time, daylight saving time counted in,
;;; whether daylight saving time is in effect (1 or 0), and what the zone is
;;; called then. Returns NIL if that is not known.
(ct:defun-kernel os-time-zone ((year :long) (month :long) (day :long)
		(hour :long) (minute :long) (second :long)
		(seconds-west (:long *)) (daylight (:long *))
		(name (:unsigned-char *)) (name-size :unsigned-long))
   :return-type :long-bool
   :kernel-name cl::%os-time-zone
   :linkage-type :c)

;;;
;;;	The process
;;;

;;; Ends the process. Does not return.
(ct:defun-kernel os-exit-process ((code :unsigned-long))
   :return-type :void
   :kernel-name cl::%os-exit-process
   :linkage-type :c)

;;;
;;;	Memory
;;;

;;; Memory by the byte, which foreign code may be handed.
(ct:defun-kernel os-heap-allocate ((size :unsigned-long))
   :return-type (:void *)
   :kernel-name cl::%os-heap-allocate
   :linkage-type :c)

(ct:defun-kernel os-heap-free ((memory (:void *)))
   :return-type :void
   :kernel-name cl::%os-heap-free
   :linkage-type :c)

;;; Whether SIZE bytes at the address can be read.
(ct:defun-kernel os-can-read ((address (:void *)) (size :unsigned-long))
   :return-type :long-bool
   :kernel-name cl::%os-can-read
   :linkage-type :c)

;;;
;;;	The operating system's code for why the last call to it by this thread
;;;	failed, and that reason in words.
;;;
(ct:defun-kernel os-last-error ()
   :return-type :unsigned-long
   :kernel-name cl::%os-last-error
   :linkage-type :c)

(defun os-error-text (code)
	#+os-windows (win:system-error-text code)
	#-os-windows (format nil "error code = ~A" code))
