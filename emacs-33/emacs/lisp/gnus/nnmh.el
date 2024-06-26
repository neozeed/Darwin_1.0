;;; nnmh.el --- mhspool access for Gnus
;; Copyright (C) 1995,96,97 Free Software Foundation, Inc.

;; Author: Lars Magne Ingebrigtsen <larsi@ifi.uio.no>
;; 	Masanobu UMEDA <umerin@flab.flab.fujitsu.junet>
;; Keywords: news, mail

;; This file is part of GNU Emacs.

;; GNU Emacs is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to the
;; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;;; Commentary:

;; Based on nnspool.el by Masanobu UMEDA <umerin@flab.flab.fujitsu.junet>.
;; For an overview of what the interface functions do, please see the
;; Gnus sources.

;;; Code:

(require 'nnheader)
(require 'nnmail)
(require 'gnus-start)
(require 'nnoo)
(eval-when-compile (require 'cl))

(nnoo-declare nnmh)

(defvoo nnmh-directory message-directory
  "*Mail spool directory.")

(defvoo nnmh-get-new-mail t
  "*If non-nil, nnmh will check the incoming mail file and split the mail.")

(defvoo nnmh-prepare-save-mail-hook nil
  "*Hook run narrowed to an article before saving.")

(defvoo nnmh-be-safe nil
  "*If non-nil, nnmh will check all articles to make sure whether they are new or not.")



(defconst nnmh-version "nnmh 1.0"
  "nnmh version.")

(defvoo nnmh-current-directory nil
  "Current news group directory.")

(defvoo nnmh-status-string "")
(defvoo nnmh-group-alist nil)



;;; Interface functions.

(nnoo-define-basics nnmh)

(deffoo nnmh-retrieve-headers (articles &optional newsgroup server fetch-old)
  (save-excursion
    (set-buffer nntp-server-buffer)
    (erase-buffer)
    (let* ((file nil)
	   (number (length articles))
	   (large (and (numberp nnmail-large-newsgroup)
		       (> number nnmail-large-newsgroup)))
	   (count 0)
	   ;; 1997/8/12 by MORIOKA Tomohiko
	   (file-name-coding-system 'binary) ; for Emacs 20
	   (pathname-coding-system 'binary)  ; for XEmacs/mule
	   beg article)
      (nnmh-possibly-change-directory newsgroup server)
      ;; We don't support fetching by Message-ID.
      (if (stringp (car articles))
	  'headers
	(while articles
	  (when (and (file-exists-p
		      (setq file (concat (file-name-as-directory
					  nnmh-current-directory)
					 (int-to-string
					  (setq article (pop articles))))))
		     (not (file-directory-p file)))
	    (insert (format "221 %d Article retrieved.\n" article))
	    (setq beg (point))
	    (nnheader-insert-head file)
	    (goto-char beg)
	    (if (search-forward "\n\n" nil t)
		(forward-char -1)
	      (goto-char (point-max))
	      (insert "\n\n"))
	    (insert ".\n")
	    (delete-region (point) (point-max)))
	  (setq count (1+ count))

	  (and large
	       (zerop (% count 20))
	       (message "nnmh: Receiving headers... %d%%"
			(/ (* count 100) number))))

	(when large
	  (message "nnmh: Receiving headers...done"))

	(nnheader-fold-continuation-lines)
	'headers))))

(deffoo nnmh-open-server (server &optional defs)
  (nnoo-change-server 'nnmh server defs)
  (when (not (file-exists-p nnmh-directory))
    (condition-case ()
	(make-directory nnmh-directory t)
      (error t)))
  (cond
   ((not (file-exists-p nnmh-directory))
    (nnmh-close-server)
    (nnheader-report 'nnmh "Couldn't create directory: %s" nnmh-directory))
   ((not (file-directory-p (file-truename nnmh-directory)))
    (nnmh-close-server)
    (nnheader-report 'nnmh "Not a directory: %s" nnmh-directory))
   (t
    (nnheader-report 'nnmh "Opened server %s using directory %s"
		     server nnmh-directory)
    t)))

(deffoo nnmh-request-article (id &optional newsgroup server buffer)
  (nnmh-possibly-change-directory newsgroup server)
  (let ((file (if (stringp id)
		  nil
		(concat nnmh-current-directory (int-to-string id))))
	;; 1997/8/12 by MORIOKA Tomohiko
	(file-name-coding-system 'binary) ; for Emacs 20
	(pathname-coding-system 'binary)  ; for XEmacs/mule
	(nntp-server-buffer (or buffer nntp-server-buffer)))
    (and (stringp file)
	 (file-exists-p file)
	 (not (file-directory-p file))
	 (save-excursion (nnmail-find-file file))
	 (string-to-int (file-name-nondirectory file)))))

(deffoo nnmh-request-group (group &optional server dont-check)
  (let ((pathname (nnmail-group-pathname group nnmh-directory))
	;; 1997/8/12 by MORIOKA Tomohiko
	(file-name-coding-system 'binary) ; for Emacs 20
	(pathname-coding-system 'binary)  ; for XEmacs/mule.
	dir)
    (cond
     ((not (file-directory-p pathname))
      (nnheader-report
       'nnmh "Can't select group (no such directory): %s" group))
     (t
      (setq nnmh-current-directory pathname)
      (and nnmh-get-new-mail
	   nnmh-be-safe
	   (nnmh-update-gnus-unreads group))
      (cond
       (dont-check
	(nnheader-report 'nnmh "Selected group %s" group)
	t)
       (t
	;; Re-scan the directory if it's on a foreign system.
	(nnheader-re-read-dir pathname)
	(setq dir
	      (sort
	       (mapcar (lambda (name) (string-to-int name))
		       (directory-files pathname nil "^[0-9]+$" t))
	       '<))
	  (cond
	   (dir
	    (nnheader-report 'nnmh "Selected group %s" group)
	    (nnheader-insert
	     "211 %d %d %d %s\n" (length dir) (car dir)
	     (progn (while (cdr dir) (setq dir (cdr dir))) (car dir))
	     group))
	   (t
	    (nnheader-report 'nnmh "Empty group %s" group)
	    (nnheader-insert (format "211 0 1 0 %s\n" group))))))))))

(deffoo nnmh-request-scan (&optional group server)
  (nnmail-get-new-mail 'nnmh nil nnmh-directory group))

(deffoo nnmh-request-list (&optional server dir)
  (nnheader-insert "")
  (let ((file-name-coding-system 'binary)
	(pathname-coding-system 'binary)
	(nnmh-toplev
	 (file-truename (or dir (file-name-as-directory nnmh-directory)))))
    (nnmh-request-list-1 nnmh-toplev))
  (setq nnmh-group-alist (nnmail-get-active))
  t)

(defvar nnmh-toplev)
(defun nnmh-request-list-1 (dir)
  (setq dir (expand-file-name dir))
  ;; Recurse down all directories.
  (let ((dirs (and (file-readable-p dir)
		   (> (nth 1 (file-attributes (file-chase-links dir))) 2)
		   (directory-files dir t nil t)))
	dir)
    ;; Recurse down directories.
    (while (setq dir (pop dirs))
      (when (and (not (member (file-name-nondirectory dir) '("." "..")))
		 (file-directory-p dir)
		 (file-readable-p dir))
	(nnmh-request-list-1 dir))))
  ;; For each directory, generate an active file line.
  (unless (string= (expand-file-name nnmh-toplev) dir)
    (let ((files (mapcar
		  (lambda (name) (string-to-int name))
		  (directory-files dir nil "^[0-9]+$" t))))
      (when files
	(save-excursion
	  (set-buffer nntp-server-buffer)
	  (goto-char (point-max))
	  (insert
	   (format
	    "%s %d %d y\n"
	    (progn
	      (string-match
	       (regexp-quote
		(file-truename (file-name-as-directory
				(expand-file-name nnmh-toplev))))
	       dir)
	      (nnheader-replace-chars-in-string
	       (decode-coding-string (substring dir (match-end 0))
				     nnmail-pathname-coding-system)
	       ?/ ?.))
	    (apply 'max files)
	    (apply 'min files)))))))
  t)

(deffoo nnmh-request-newgroups (date &optional server)
  (nnmh-request-list server))

(deffoo nnmh-request-expire-articles (articles newsgroup
					       &optional server force)
  (nnmh-possibly-change-directory newsgroup server)
  (let* ((active-articles
	  (mapcar
	   (function
	    (lambda (name)
	      (string-to-int name)))
	   (directory-files nnmh-current-directory nil "^[0-9]+$" t)))
	 (is-old t)
	 article rest mod-time)
    (nnmail-activate 'nnmh)

    (while (and articles is-old)
      (setq article (concat nnmh-current-directory
			    (int-to-string (car articles))))
      (when (setq mod-time (nth 5 (file-attributes article)))
	(if (and (nnmh-deletable-article-p newsgroup (car articles))
		 (setq is-old
		       (nnmail-expired-article-p newsgroup mod-time force)))
	    (progn
	      (nnheader-message 5 "Deleting article %s in %s..."
				article newsgroup)
	      (condition-case ()
		  (funcall nnmail-delete-file-function article)
		(file-error
		 (nnheader-message 1 "Couldn't delete article %s in %s"
				   article newsgroup)
		 (push (car articles) rest))))
	  (push (car articles) rest)))
      (setq articles (cdr articles)))
    (message "")
    (nconc rest articles)))

(deffoo nnmh-close-group (group &optional server)
  t)

(deffoo nnmh-request-move-article
  (article group server accept-form &optional last)
  (let ((buf (get-buffer-create " *nnmh move*"))
	result)
    (and
     (nnmh-deletable-article-p group article)
     (nnmh-request-article article group server)
     (save-excursion
       (set-buffer buf)
       (erase-buffer)
       (insert-buffer-substring nntp-server-buffer)
       (setq result (eval accept-form))
       (kill-buffer (current-buffer))
       result)
     (progn
       (nnmh-possibly-change-directory group server)
       (condition-case ()
	   (funcall nnmail-delete-file-function
		    (concat nnmh-current-directory (int-to-string article)))
	 (file-error nil))))
    result))

(deffoo nnmh-request-accept-article (group &optional server last noinsert)
  (nnmh-possibly-change-directory group server)
  (nnmail-check-syntax)
  (when nnmail-cache-accepted-message-ids
    (nnmail-cache-insert (nnmail-fetch-field "message-id")))
  (prog1
      (if (stringp group)
	  (and
	   (nnmail-activate 'nnmh)
	   (car (nnmh-save-mail
		 (list (cons group (nnmh-active-number group)))
		 noinsert)))
	(and
	 (nnmail-activate 'nnmh)
	 (let ((res (nnmail-article-group 'nnmh-active-number)))
	   (if (and (null res)
		    (yes-or-no-p "Moved to `junk' group; delete article? "))
	       'junk
	     (car (nnmh-save-mail res noinsert))))))
    (when (and last nnmail-cache-accepted-message-ids)
      (nnmail-cache-close))))

(deffoo nnmh-request-replace-article (article group buffer)
  (nnmh-possibly-change-directory group)
  (save-excursion
    (set-buffer buffer)
    (nnmh-possibly-create-directory group)
    (ignore-errors
      (nnmail-write-region
       (point-min) (point-max)
       (concat nnmh-current-directory (int-to-string article))
       nil (if (nnheader-be-verbose 5) nil 'nomesg))
      t)))

(deffoo nnmh-request-create-group (group &optional server args)
  (nnmail-activate 'nnmh)
  (unless (assoc group nnmh-group-alist)
    (let (active)
      (push (list group (setq active (cons 1 0)))
	    nnmh-group-alist)
      (nnmh-possibly-create-directory group)
      (nnmh-possibly-change-directory group server)
      (let ((articles (mapcar
		       (lambda (file)
			 (string-to-int file))
		       (directory-files
			nnmh-current-directory nil "^[0-9]+$"))))
	(when articles
	  (setcar active (apply 'min articles))
	  (setcdr active (apply 'max articles))))))
  t)

(deffoo nnmh-request-delete-group (group &optional force server)
  (nnmh-possibly-change-directory group server)
  ;; Delete all articles in GROUP.
  (if (not force)
      ()				; Don't delete the articles.
    (let ((articles (directory-files nnmh-current-directory t "^[0-9]+$")))
      (while articles
	(when (file-writable-p (car articles))
	  (nnheader-message 5 "Deleting article %s in %s..."
			    (car articles) group)
	  (funcall nnmail-delete-file-function (car articles)))
	(setq articles (cdr articles))))
    ;; Try to delete the directory itself.
    (ignore-errors
      (delete-directory nnmh-current-directory)))
  ;; Remove the group from all structures.
  (setq nnmh-group-alist
	(delq (assoc group nnmh-group-alist) nnmh-group-alist)
	nnmh-current-directory nil)
  t)

(deffoo nnmh-request-rename-group (group new-name &optional server)
  (nnmh-possibly-change-directory group server)
  (let ((new-dir (nnmail-group-pathname new-name nnmh-directory))
	(old-dir (nnmail-group-pathname group nnmh-directory)))
    (when (ignore-errors
	    (make-directory new-dir t)
	    t)
      ;; We move the articles file by file instead of renaming
      ;; the directory -- there may be subgroups in this group.
      ;; One might be more clever, I guess.
      (let ((files (nnheader-article-to-file-alist old-dir)))
	(while files
	  (rename-file
	   (concat old-dir (cdar files))
	   (concat new-dir (cdar files)))
	  (pop files)))
      (when (<= (length (directory-files old-dir)) 2)
	(ignore-errors
	  (delete-directory old-dir)))
      ;; That went ok, so we change the internal structures.
      (let ((entry (assoc group nnmh-group-alist)))
	(when entry
	  (setcar entry new-name))
	(setq nnmh-current-directory nil)
	t))))

(nnoo-define-skeleton nnmh)


;;; Internal functions.

(defun nnmh-possibly-change-directory (newsgroup &optional server)
  (when (and server
	     (not (nnmh-server-opened server)))
    (nnmh-open-server server))
  (when newsgroup
    (let ((pathname (nnmail-group-pathname newsgroup nnmh-directory))
	  ;; 1997/8/12 by MORIOKA Tomohiko
	  (file-name-coding-system 'binary) ; for Emacs 20
	  (pathname-coding-system 'binary)) ; for XEmacs/mule
      (if (file-directory-p pathname)
	  (setq nnmh-current-directory pathname)
	(error "No such newsgroup: %s" newsgroup)))))

(defun nnmh-possibly-create-directory (group)
  (let (dir dirs)
    (setq dir (nnmail-group-pathname group nnmh-directory))
    (while (not (file-directory-p dir))
      (push dir dirs)
      (setq dir (file-name-directory (directory-file-name dir))))
    (while dirs
      (when (make-directory (directory-file-name (car dirs)))
	(error "Could not create directory %s" (car dirs)))
      (nnheader-message 5 "Creating mail directory %s" (car dirs))
      (setq dirs (cdr dirs)))))

(defun nnmh-save-mail (group-art &optional noinsert)
  "Called narrowed to an article."
  (unless noinsert
    (nnmail-insert-lines)
    (nnmail-insert-xref group-art))
  (run-hooks 'nnmail-prepare-save-mail-hook)
  (run-hooks 'nnmh-prepare-save-mail-hook)
  (goto-char (point-min))
  (while (looking-at "From ")
    (replace-match "X-From-Line: ")
    (forward-line 1))
  ;; We save the article in all the newsgroups it belongs in.
  (let ((ga group-art)
	first)
    (while ga
      (nnmh-possibly-create-directory (caar ga))
      (let ((file (concat (nnmail-group-pathname
			   (caar ga) nnmh-directory)
			  (int-to-string (cdar ga)))))
	(if first
	    ;; It was already saved, so we just make a hard link.
	    (funcall nnmail-crosspost-link-function first file t)
	  ;; Save the article.
	  (nnmail-write-region (point-min) (point-max) file nil nil)
	  (setq first file)))
      (setq ga (cdr ga))))
  group-art)

(defun nnmh-active-number (group)
  "Compute the next article number in GROUP."
  (let ((active (cadr (assoc group nnmh-group-alist)))
	(dir (nnmail-group-pathname group nnmh-directory))
	;; 1997/8/14 by MORIOKA Tomohiko
	(file-name-coding-system 'binary) ; for Emacs 20
	(pathname-coding-system 'binary)) ; for XEmacs/mule
    (unless active
      ;; The group wasn't known to nnmh, so we just create an active
      ;; entry for it.
      (setq active (cons 1 0))
      (push (list group active) nnmh-group-alist)
      (unless (file-exists-p dir)
	(make-directory dir))
      ;; Find the highest number in the group.
      (let ((files (sort
		    (mapcar
		     (lambda (f)
		       (string-to-int f))
		     (directory-files dir nil "^[0-9]+$"))
		    '>)))
	(when files
	  (setcdr active (car files)))))
    (setcdr active (1+ (cdr active)))
    (while (file-exists-p
	    (concat (nnmail-group-pathname group nnmh-directory)
		    (int-to-string (cdr active))))
      (setcdr active (1+ (cdr active))))
    (cdr active)))

(defun nnmh-update-gnus-unreads (group)
  ;; Go through the .nnmh-articles file and compare with the actual
  ;; articles in this folder.  The articles that are "new" will be
  ;; marked as unread by Gnus.
  (let* ((dir nnmh-current-directory)
	 (files (sort (mapcar (function (lambda (name) (string-to-int name)))
			      (directory-files nnmh-current-directory
					       nil "^[0-9]+$" t))
		      '<))
	 (nnmh-file (concat dir ".nnmh-articles"))
	 new articles)
    ;; Load the .nnmh-articles file.
    (when (file-exists-p nnmh-file)
      (setq articles
	    (let (nnmh-newsgroup-articles)
	      (ignore-errors (load nnmh-file nil t t))
	      nnmh-newsgroup-articles)))
    ;; Add all new articles to the `new' list.
    (let ((art files))
      (while art
	(unless (assq (car art) articles)
	  (push (car art) new))
	(setq art (cdr art))))
    ;; Remove all deleted articles.
    (let ((art articles))
      (while art
	(unless (memq (caar art) files)
	  (setq articles (delq (car art) articles)))
	(setq art (cdr art))))
    ;; Check whether the articles really are the ones that Gnus thinks
    ;; they are by looking at the time-stamps.
    (let ((arts articles)
	  art)
      (while (setq art (pop arts))
	(when (not (equal
		    (nth 5 (file-attributes
			    (concat dir (int-to-string (car art)))))
		    (cdr art)))
	  (setq articles (delq art articles))
	  (push (car art) new))))
    ;; Go through all the new articles and add them, and their
    ;; time-stamps, to the list.
    (setq articles
	  (nconc articles
		 (mapcar
		  (lambda (art)
		    (cons art
			  (nth 5 (file-attributes
				  (concat dir (int-to-string art))))))
		  new)))
    ;; Make Gnus mark all new articles as unread.
    (when new
      (gnus-make-articles-unread
       (gnus-group-prefixed-name group (list 'nnmh ""))
       (setq new (sort new '<))))
    ;; Sort the article list with highest numbers first.
    (setq articles (sort articles (lambda (art1 art2)
				    (> (car art1) (car art2)))))
    ;; Finally write this list back to the .nnmh-articles file.
    (nnheader-temp-write nnmh-file
      (insert ";; Gnus article active file for " group "\n\n")
      (insert "(setq nnmh-newsgroup-articles '")
      (gnus-prin1 articles)
      (insert ")\n"))))

(defun nnmh-deletable-article-p (group article)
  "Say whether ARTICLE in GROUP can be deleted."
  (let ((path (concat nnmh-current-directory (int-to-string article))))
    ;; Writable.
    (and (file-writable-p path)
	 ;; We can never delete the last article in the group.
	 (not (eq (cdr (nth 1 (assoc group nnmh-group-alist)))
		  article)))))

(provide 'nnmh)

;;; nnmh.el ends here
