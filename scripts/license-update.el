#!/usr/bin/emacs --script
;;; -*- Mode: Emacs-Lisp -*-

(require 'cl)

(defvar *js-source-directory-exclude* '("." ".." "contrib" ".sconf_temp"))

(defvar *js-source-regexps* '(
			      ;; C/C++ sources and headers
			      ".*\\.\\(cc?\\|hh?\\)$"))

(defvar *js-copyright-text* "Copyright (C) %s, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
Economic rights: Technische Universitaet Dresden (Germany)

This file is part of sv3.

sv3 is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

sv3 is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License version 2 for more details.
")

(defun js-file-matches-any (fname regexes)
  (some (lambda (r) (string-match r fname)) regexes))

(defun* js-recursive-file-list (directory
				&optional (regexps *js-source-regexps*) results)
  "Returns a list of all files in the given directory and its
  subdirectories with on of the given `regexps'."
  (dolist (fitem (directory-files-and-attributes directory t) results)
    (destructuring-bind (fname dir? . rest)
	fitem
      (if dir?
	  ;; Recurse if directory and not a dot directory.
	  (unless (find (file-name-nondirectory fname)
			*js-source-directory-exclude*
			:test 'string=)
	    (setq results (js-recursive-file-list fname regexps results)))
	(if (js-file-matches-any fname regexps)
	    (push fname results))))))

(defun js-point-in-comment-p ()
  "Returns true, iff point is inside a comment."
  (elt (syntax-ppss) 4))

(defun* js-has-copyright-header ()
  (save-excursion
    (goto-char (point-min))
    (while (search-forward "Copyright (C)" nil t)
      ;; Do something
      (if (js-point-in-comment-p)
	  (return-from js-has-copyright-header t))))
  nil)

(defun js-current-line ()
  "Returns the line at point as string."
  (buffer-substring-no-properties (line-beginning-position)
				  (line-end-position)))

(defun js-line-is-mode-line-p ()
  (string-match "-\\*-.*-\\*-" (js-current-line)))

(defun js-add-copyright-header ()
  (save-excursion
    (goto-char (point-min))
    (when (js-line-is-mode-line-p)
      (forward-line))
    (let ((beginning (point)))
      (insert (format *js-copyright-text* (format-time-string "%Y") ))
      (comment-region beginning (point))
      (insert "\n"))))

(dolist (dir command-line-args-left)
  (message "Processing %s ..." dir)
  (save-current-buffer
    (dolist (file (js-recursive-file-list dir))
      (find-file file)
      (unwind-protect
	  (if (js-has-copyright-header)
	      (message "Has copyright header: %s" file)
	    ;; No header found.
	    (message "Adding header: %s" file)
	    (js-add-copyright-header)
	    (save-buffer 16))
	(kill-buffer)))))

;;; EOF
