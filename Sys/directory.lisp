;;;;	-------------------------------
;;;;	Copyright (c) Corman Technologies Inc.
;;;;	See LICENSE.txt for license information.
;;;;	-------------------------------
;;;;
;;;;	File:		directory.lisp
;;;;	Contents:	Implementation of DIRECTORY function.
;;;;	History:	1/28/98  RGC  Created.
;;;;				11/11/98 RGC  Modified DIRECTORY to return full (absolute) paths.
;;;;				2/22/01  RGC  Added CCL:CURRENT-DIRECTORY, (SETF CCL:CURRENT-DIRECTORY)
;;;;				8/16/01  RGC  Current directory functions now accept any path designator.
;;;;				10/10/01 RGC  Added JP Massar's RENAME-FILE implementation.
;;;;                9/19/03  RGC  Incorporated JP Massar's enhanced directory functionality.
;;;;                4/19/06  RGC  Modified by Edi Weitz to set the *default-pathname-defaults* 
;;;;                              to the new directory.
;;;;                              Incorporated Edi's mod to SUBDIRECTORY-PATHNAME.
;;;;

;; requires win32.lisp to be loaded first
(in-package :win32)

(defun subdirectory-pathname 
    (directory-pathname subdir-name &key (name nil) (type nil) (version nil))
  (let ((p1 (parse-namestring subdir-name)))
    (truename
     (make-pathname
      :host (pathname-host directory-pathname)
      :device (pathname-device directory-pathname)
      :directory 
      (append 
       (pathname-directory directory-pathname) 
       (list (if (pathname-type p1)
               (format nil "~A.~A" (pathname-name p1) (pathname-type p1))
               (pathname-name p1))))
      :name name
      :type type
      :version version
      ))))

(defun file-pathname (directory-pathname file-name &key (version nil))
  (let ((p1 (parse-namestring file-name)))
    (truename
     (make-pathname
      :host (pathname-host directory-pathname)
      :device (pathname-device directory-pathname)
      :directory (pathname-directory directory-pathname)
      :name (pathname-name p1)
      :type (pathname-type p1)
      :version version
      ))))

;;; forward declaration
(defun cl::directory (pathspec &key (recurse nil))
    (declare (ignore pathspec recurse)))
    
(defun cl::directory-internal (pathspec files? subdirs? recurse?)

  (let* ((fileinfo (ct:malloc (sizeof 'cl::os-found-file)))
	 (file-list nil)
	 (subdir-list nil)
	 (subdir-name-list nil)
	 (pathname (truename pathspec))
	 (name-part (pathname-name pathname))
	 (extension-part (pathname-type pathname)))

    (when files?
      (do* ((handle (cl::os-find-first-file (namestring pathname) fileinfo))
	    (found (/= handle 0) (cl::os-find-next-file handle fileinfo)))
	  ((not found)(cl::os-find-close handle))
	(let ((name 
	       (ct:c-string-to-lisp-string (cref cl::os-found-file fileinfo cl::name))))
	  (when (= (cref cl::os-found-file fileinfo cl::is-directory) 0)  ;; if not a subdirectory
	    (push (file-pathname pathname name) file-list)
	    ))))
	    
    ;; go through subdirectories now

    (when (or subdirs? recurse?)
      (let ((dirpath (make-pathname
		      :host (pathname-host pathname)
		      :device (pathname-device pathname)
		      :directory (pathname-directory pathname)
		      :name "*"
		      :type "*")))
	(do* ((handle (cl::os-find-first-file (namestring dirpath) fileinfo))
	      (found (/= handle 0) (cl::os-find-next-file handle fileinfo)))
	    ((not found)(cl::os-find-close handle))
	  (let ((name (ct:c-string-to-lisp-string 
		       (cref cl::os-found-file fileinfo cl::name))))
	    (when (/= (cref cl::os-found-file fileinfo cl::is-directory) 0)
	      (unless (or (string= name ".") (string= name ".."))
		(push (subdirectory-pathname dirpath name) subdir-list)
		(push name subdir-name-list)
		))))))
    
    (setq subdir-list (nreverse subdir-list))
    (setq subdir-name-list (nreverse subdir-name-list))

    (when recurse?
      (dolist (subdir-name subdir-name-list)
	(unless (or (string= subdir-name ".") (string= subdir-name ".."))
	  (let* ((subdir-path
		  (subdirectory-pathname 
		      pathname subdir-name
		      :name name-part :type extension-part
		      ))
		 (files (cl::directory (truename subdir-path) :recurse t)))
	    (if files (push files file-list))
	    ))))
    
    (setq file-list (nreverse file-list))
    
    (values file-list subdir-list)
    
    ))

(defun cl::directory-files (pathspec &key (recurse nil))
  (multiple-value-bind (files subdirs)
      (cl::directory-internal pathspec t nil recurse)
    (declare (ignore subdirs))
    files
    ))

(defun cl::directory-subdirs (pathspec)
  (multiple-value-bind (files subdirs)
      (cl::directory-internal pathspec nil t nil)
    (declare (ignore files))
    subdirs
    ))

;; Returns two values, a list of file pathnames and a list of subdir pathnames
(defun cl::directory-files-and-subdirs (pathspec)
  (cl::directory-internal pathspec t t nil)
  )

;;;
;;; Common Lisp DIRECTORY function.
;;; New implementation, adds :RECURSE keyword.
;;;
(defun cl::directory (pathspec &key (recurse nil))
    (cl::directory-files pathspec :recurse recurse))
		
(defun ccl::get-current-directory ()
	(let* ((buf (ct:malloc 256))
		   (ret (cl::os-current-directory buf 256)))
		(if (not ret)
			(error "Could not get current directory")
			(let* ((str (ct:c-string-to-lisp-string buf))
				   (last-char (char str (- (length str) 1))))
				;; make sure the directory ends with a slash
				(unless (or (char= last-char #\\) (char= last-char #\/))
					(setf str (concatenate 'string str cl::os-directory-separator)))
				(values (parse-namestring str))))))

;;;
;;; Modified by Edi Weitz to set the *default-pathname-defaults* to the new directory
;;;
(defun ccl::set-current-directory (dir) 
  (let ((ret (cl::os-set-current-directory (namestring dir))))
    (if (not ret)
      (error "Could not set current directory to ~A" dir)
      (setq *default-pathname-defaults*
              (pl::get-current-directory)))))		

(defun ccl::current-directory () (ccl::get-current-directory))
(defun (setf ccl::current-directory) (dir)
	(ccl::set-current-directory dir))

;;;
;;;	Common Lisp ENSURE-DIRECTORIES-EXIST function
;;;
(defun ensure-directories-exist (pathspec &key verbose)
	(let* ((path (truename pathspec))
		   (dirs (cdr (pathname-directory path)))
		   (created nil)
		   (dir (list ':absolute)))
		(dolist (x dirs)
			(setf dir (append dir (list x)))
			(setf path 
				(make-pathname :device (pathname-device path)
							   :directory dir))
			(when (cl::os-create-directory (ct:lisp-string-to-c-string (namestring path)))
				(setf created t)
				(if verbose 
					(format t "~&Created directory ~A~%" (namestring path)))))
		(values pathspec created)))

;;; Implementation of RENAME-FILE for Corman Lisp.
;;; JP Massar.  10/9/01.
;;; Put in  VERBOSE keyword.

(defun MoveFile (old new)
  (cl::os-move-file old new))

;;;
;;;	Common Lisp RENAME-FILE function
;;;
(defun common-lisp:rename-file (old new &key (verbose nil))
  
  (let* ((old-pathname
          (merge-pathnames (pathname old) *default-pathname-defaults*))
         (new-pathname
          (merge-pathnames (pathname new) old-pathname)))
    
    (flet 
        ((verify-for-rename
          (path)
          (let ((name (pathname-name path))
                (type (pathname-type path))
                (dir (pathname-directory path))
                )
            (when (and dir 
                       (or (null name) (eq name :unspecific))
                       (or (null type) (eq type :unspecific))
                       )
              (error "Invalid path: ~A. ~% ~
                      Use a pathname with a :NAME component.~% ~
                      (If you are trying to rename a directory specify the~% ~
                      path as a string without a trailing slash.)"
                path
                ))
            (when (or ;(wild-device-or-directory-component? path)
                      (eq name :wild) 
                      (eq :name :unspecific)
                      (null name)
                      (and (stringp name) (string= name "*"))
                      (eq type :wild)
                      (and (stringp type) (string= type "*"))
                      )
              (error "Invalid component in pathname: ~A" path)
              ))))
      
      (verify-for-rename old-pathname)
      (verify-for-rename new-pathname)
      
      (unless (or (probe-file old-pathname)
                  (ccl::directory-p old-pathname)
                  )
        (error "File to be renamed does not exist: ~A" old-pathname)
        )
      
      (let* ((true-old (truename old-pathname))
             (old-name (namestring true-old))
             (new-name (namestring new-pathname))
             )
        
        (when verbose 
          (fresh-line)
          (format t "Renaming '~A' to '~A'~%" old-name new-name))
        
        (if (null (MoveFile (ct:lisp-string-to-c-string old-name) 
                            (ct:lisp-string-to-c-string new-name)))
            (error "Could not rename file.  'MoveFile' OS call returned error code.")
          (values
           new-pathname 
           true-old
           (truename new-pathname)
           ))
        
        ))))

;; Make sure the default pathname gets set when an image is loaded.
(flet ((init-default-path ()
			(setq cl::*default-pathname-defaults* (ccl::get-current-directory))))
	(init-default-path)
	(cl::register-load-image-restore-func #'init-default-path))

(export '(ccl::get-current-directory ccl::set-current-directory ccl::current-directory) "CORMANLISP")
		