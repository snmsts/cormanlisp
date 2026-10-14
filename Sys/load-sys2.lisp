;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		load-sys2.lisp
;;;;	Contents:	Corman Lisp code to build the system.
;;;;	History:	6/24/96  RGC  Created.
;;;;

(defun load-file (filename)
    (if (eq (cl::cormanlisp-client-type) 2)
        (editor-set-message (format nil "Compiling ~a" filename))
        (progn (format t "Compiling ~a~%" filename)(force-output)))
    (load filename))
    
(setq cl::*compiler-save-lambdas* nil)
(setq cl::*compiler-save-table-references* nil)

(load-file "Sys/declarations.lisp")
(load-file "Sys/kernel-asm.lisp")
(load-file "Sys/kernel-funcs.lisp")
(load-file "Sys/trees.lisp")
(load-file "Sys/compiler.lisp")
(load-file "Sys/lists.lisp")
(load-file "Sys/characters.lisp")
(load-file "Sys/strings.lisp")
(load-file "Sys/math.lisp")
(load-file "Sys/random.lisp")
(load-file "Sys/symbols.lisp")
(load-file "Sys/control-structures.lisp")
(load-file "Sys/sequences.lisp")
(load-file "Sys/subtypep.lisp")
(load-file "Sys/coerce.lisp")
(load-file "Sys/input-output.lisp")
(load-file "Sys/errors.lisp")
(load-file "Sys/defpackage.lisp")
(load-file "Sys/misc-features.lisp")
(load-file "Sys/clos.lisp")
(load-file "Sys/fast-class-of.lisp")
(load-file "Sys/conditions.lisp")
(load-file "Sys/tail-calls.lisp")
(load-file "Sys/profiler.lisp")
(load-file "Sys/ffi.lisp")
(load-file "Sys/trace.lisp")

(defun open ())			; avoid warnings
(defun ct::read-token ()) ; avoid warnings

(load-file "Sys/parse-c-decls.lisp")
(load-file "Sys/win32.lisp")
(load-file "Sys/win-conditions.lisp")
(load-file "Sys/os.lisp")
#+os-windows (load-file "Sys/com.lisp")
#+os-windows (load-file "Sys/winsock.lisp")
#-os-windows (load-file "Sys/bsd-sockets.lisp")	; which is the package WINSOCK there
(load-file "Sys/time.lisp")
(load-file "Sys/math2.lisp")
(load-file "Sys/filenames.lisp")
(load-file "Sys/streams.lisp")
(load-file "Sys/autoload.lisp")

(load-file "Sys/loop.lisp")
(load-file "Sys/describe.lisp")
(load-file "Sys/pretty.lisp")
(load-file "Sys/directory.lisp")
(load-file "Sys/open-file.lisp")
(load-file "Sys/menus.lisp")
(load-file "Sys/registry.lisp")
#+os-windows (load-file "Sys/edit-window.lisp")
#-os-windows (defpackage "IDE")		; which files loaded later expect to be there
(load-file "Sys/imagehlp.lisp")
(load-file "Sys/map-file.lisp")
(load-file "Sys/compile-file.lisp")
(load-file "Sys/debug.lisp")
(load-file "Sys/save-application.lisp")
(load-file "Sys/dribble.lisp")
(load-file "Sys/require.lisp")
(load-file "Sys/documentation.lisp")
(load-file "Sys/print-float.lisp")
(load-file "Sys/bits.lisp")
(load-file "Sys/boole.lisp")
(load-file "Sys/bignums.lisp")
(load-file "Sys/math-ops.lisp")
(load-file "Sys/places.lisp")
(load-file "Sys/misc-utility.lisp")
;; load code formatting engine
(let ((*package* (find-package :ide)))
  (with-input-from-string (in "") ;; to not let it hang, as it calls INDENT-LINES
    (let ((*standard-input* in))
      (load-file (concatenate 'string ccl::*cormanlisp-directory* "Sys/scmindent/lispindent.lisp"))))
  ;; use Common Lisp style indenting for the IF form
  (setf (cdr (assoc "IF" ide::*lisp-keywords* :test #'string-equal)) -1))
(load-file "Sys/code-indenter.lisp")
#+os-windows (load-file "Sys/context-menu.lisp")
(load-file "Sys/setf-expander.lisp")
(load-file "Sys/sockets.lisp")
(load-file "Sys/xp.lisp")
(load-file "Sys/threads.lisp")
(load-file "Sys/version.lisp")
#+os-windows (load-file "Sys/auto-update.lisp")
#+os-windows (load-file "Sys/jumpmenu.lisp")
#+os-windows (load-file "Sys/ide-menus.lisp")

;; Load patches
(in-package :cl)
#+os-windows
(let ((patch-files
        (sort (directory 
                    (merge-pathnames "CormanLisp_3_0_patch_??.lisp" (ccl::local-patches-directory)))
            #'string< :key 'namestring)))
    (dolist (f (mapcar 'namestring patch-files))
        (let ((ccl::*patch* (ccl::make-patch f)))
            (load-file f))))

(export 'ccl::find-in-files :ccl)
(ccl:define-autoloaded-module "Modules/find-in-files.lisp" (:functions ccl::find-in-files))

;; QUASILOAD will replace all the functions and macros with thunks,
;; making the originals garbage-collectable.
;(show-message "Making PARSE-C-DECLS.LISP autoloadable")
;(ccl:quasiload "sys\\parse-c-decls.lisp")
;; Demote the ct::+c-keywords+ constant defined by the parser to a
;; mutable variable to avoid a warning when the autoload above fires.
(%symbol-set-flags  
	(logandc2 (%symbol-get-flags 'ct::+c-keywords+) *symbol-constant-flag*)
	'ct::+c-keywords+)

(do-symbols (sym (find-package :keyword))
	(cl::symbol-set-constant-flag sym)
	(cl::symbol-set-special-flag sym))		;; keywords defined in the kernel need to be made constant

(editor-set-default-message)

(in-package :cl-user)
(setq cl::*compiler-save-lambdas* t)
(setq cl::*compiler-save-table-references* t)
(setq cl::*loading-kernel* nil)
(setq cl::*compress-img* t)


