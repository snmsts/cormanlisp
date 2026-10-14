;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		load-sys.lisp
;;;;	Contents:	Corman Lisp code to build the system.
;;;;	History:	6/24/96  RGC  Created.
;;;;

(setq *compiler-save-lambdas* nil)
(setq *compiler-save-table-references* nil)
(setq *append-refs-to-code* t)
(editor-set-message "Loading BOOTSTRAP.LISP")	(load "Sys/bootstrap.lisp")
;(setq *COMPILE-VERBOSE* t)
(editor-set-message "Loading EXPAND.LISP")		(load "Sys/expand.lisp")
(editor-set-message "Loading UVECTOR.LISP")		(load "Sys/uvector.lisp")
(editor-set-message "Loading READ.LISP")		(load "Sys/read.lisp")
(editor-set-message "Loading MISC.LISP")		(load "Sys/misc.lisp")
(editor-set-message "Loading READTABLE.LISP")	(load "Sys/readtable.lisp")
(editor-set-message "Loading MASSAGE.LISP")		(load "Sys/massage.lisp")
(editor-set-message "Loading WRITE.LISP")		(load "Sys/write.lisp")

(editor-set-message "Loading DESTRUCTURE.LISP")	(load "Sys/destructure.lisp")
(editor-set-message "Loading UTIL.LISP")		(load "Sys/util.lisp")
(editor-set-message "Loading SEQUENCE.LISP")	(load "Sys/sequence.lisp")
(editor-set-message "Loading FORMAT.LISP")		(load "Sys/format.lisp")

(defun load-file (filename)
    (if (eq (cormanlisp-client-type) 2)
        (editor-set-message (format nil "Compiling ~a" filename))
        (progn (format t "Compiling ~a~%" filename)(force-output)))
    (load filename))

(setf *compiler-warn-on-unused-variable* t)
(load-file "Sys/types.lisp")
(load-file "Sys/io.lisp")
(load-file "Sys/clmacros.lisp")
(load-file "Sys/backquote.lisp")
(load-file "Sys/package.lisp")
(load-file "Sys/pl-imports.lisp")
(load-file "Sys/setf.lisp")
(load-file "Sys/structures.lisp")
(load-file "Sys/array.lisp")
(load-file "Sys/arrays.lisp")
(load-file "Sys/assembler.lisp")
(load-file "Sys/hash-table.lisp")
(load-file "Sys/toplevel.lisp")
(editor-set-default-message)
(setq cl::*compiler-save-lambdas* t)
(setq cl::*compiler-save-table-references* t)
