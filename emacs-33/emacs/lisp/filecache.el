;;; filecache.el --- Find files using a pre-loaded cache
;;
;; Author:  Peter Breton <pbreton@cs.umb.edu>
;; Created: Sun Nov 10 1996
;; Keywords: convenience
;; Time-stamp: <1998-04-29 22:38:56 pbreton>
;;
;; Copyright (C) 1996 Free Software Foundation, Inc.

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
;;
;; The file-cache package is an attempt to make it easy to locate files
;; by name, without having to remember exactly where they are located.
;; This is very handy when working with source trees. You can also add
;; frequently used files to the cache to create a hotlist effect.
;; The cache can be used with any interactive command which takes a
;; filename as an argument.
;;
;; It is worth noting that this package works best when most of the files
;; in the cache have unique names, or (if they have the same name) exist in
;; only a few directories. The worst case is many files all with
;; the same name and in different directories, for example a big source tree
;; with a Makefile in each directory. In such a case, you should probably
;; use an alternate strategy to find the files.
;;
;; ADDING FILES TO THE CACHE:
;;
;; Use the following functions to add items to the file cache:
;; 
;;   * `file-cache-add-file': Adds a single file to the cache
;;
;;   * `file-cache-add-file-list': Adds a list of files to the cache
;;
;; The following functions use the regular expressions in
;; `file-cache-delete-regexps' to eliminate unwanted files:
;;   
;;   * `file-cache-add-directory': Adds the files in a directory to the
;;     cache. You can also specify a regular expression to match the files
;;     which should be added.
;;
;;   * `file-cache-add-directory-list': Same as above, but acts on a list
;;     of directories. You can use `load-path', `exec-path' and the like.
;;
;;   * `file-cache-add-directory-using-find': Uses the `find' command to
;;     add a directory tree to the cache.
;;
;;   * `file-cache-add-directory-using-locate': Uses the `locate' command to
;;     add files matching a pattern to the cache.
;;
;; Use the function `file-cache-clear-cache' to remove all items from the
;; cache. There are a number of `file-cache-delete' functions provided
;; as well, but in general it is probably better to not worry too much
;; about extra files in the cache.
;;
;; The most convenient way to initialize the cache is with an
;; `eval-after-load' function, as noted in the ADDING FILES
;; AUTOMATICALLY section.
;;
;; FINDING FILES USING THE CACHE:
;;
;; You can use the file-cache with any function that expects a filename as
;; an argument. For example:
;;
;; 1) Invoke a function which expects a filename as an argument:
;;    M-x find-file
;;
;; 2) Begin typing a file name.
;;
;; 3) Invoke `file-cache-minibuffer-complete' (bound by default to
;; C-TAB) to complete on the filename using the cache.
;;
;; 4) When you have found a unique completion, the minibuffer contents
;; will change to the full name of that file.
;; 
;; If there are a number of directories which contain the completion,
;; invoking `file-cache-minibuffer-complete' repeatedly will cycle through
;; them.
;;
;; 5) You can then edit the minibuffer contents, or press RETURN.
;;
;; It is much easier to simply try it than trying to explain it :)
;;
;;; ADDING FILES AUTOMATICALLY
;;
;; For maximum utility, you should probably define an `eval-after-load'
;; form which loads your favorite files:
;;
;;      (eval-after-load 
;;       "filecache"
;;       '(progn
;; 	    (message "Loading file cache...")
;; 	    (file-cache-add-directory-using-find "~/projects")
;;	    (file-cache-add-directory-list load-path)
;;	    (file-cache-add-directory "~/")
;;	    (file-cache-add-file-list (list "~/foo/bar" "~/baz/bar"))
;; 	   ))
;;
;; If you clear and reload the cache frequently, it is probably easiest
;; to put your initializations in a function:
;;
;;   (eval-after-load 
;;     "filecache"
;;      '(my-file-cache-initialize))
;;        
;;   (defun my-file-cache-initialize ()
;;      (interactive)
;; 	(message "Loading file cache...")
;; 	(file-cache-add-directory-using-find "~/projects")
;;	(file-cache-add-directory-list load-path)
;;	(file-cache-add-directory "~/")
;;	(file-cache-add-file-list (list "~/foo/bar" "~/baz/bar"))
;;   ))
;;
;; Of course, you can still add files to the cache afterwards, via
;; Lisp functions.
;;
;; RELATED WORK:
;; 
;; This package is a distant relative of Noah Friedman's fff utilities.
;; Our goal is pretty similar, but the implementation strategies are
;; different.

;;; Code:

(defgroup file-cache nil
  "Find files using a pre-loaded cache."
  :group 'files
  :group 'convenience
  :prefix "file-cache-")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Customization Variables
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; User-modifiable variables
(defcustom file-cache-filter-regexps 
  (list "~$" "\\.o$" "\\.exe$" "\\.a$" "\\.elc$" ",v$" "\\.output$" 
	"\\.$" "#$")
  "*List of regular expressions used as filters by the file cache.
File names which match these expressions will not be added to the cache.
Note that the functions `file-cache-add-file' and `file-cache-add-file-list' 
do not use this variable."
  :type '(repeat regexp)
  :group 'file-cache)

(defcustom file-cache-find-command "find"
  "*External program used by `file-cache-add-directory-using-find'."
  :type 'string
  :group 'file-cache)

(defcustom file-cache-locate-command "locate"
  "*External program used by `file-cache-add-directory-using-locate'."
  :type 'string
  :group 'file-cache)

;; Minibuffer messages
(defcustom file-cache-no-match-message " [File Cache: No match]"
  "Message to display when there is no completion."
  :type 'string
  :group 'file-cache)

(defcustom file-cache-sole-match-message " [File Cache: sole completion]"
  "Message to display when there is only one completion."
  :type 'string
  :group 'file-cache)

(defcustom file-cache-non-unique-message
  " [File Cache: complete but not unique]"
  "Message to display when there is a non-unique completion."
  :type 'string
  :group 'file-cache)

(defvar file-cache-multiple-directory-message nil)

;; Internal variables
;; This should be named *Completions* because that's what the function
;; switch-to-completions in simple.el expects
(defcustom file-cache-completions-buffer "*Completions*"
  "Buffer to display completions when using the file cache."
  :type 'string
  :group 'file-cache)

(defcustom file-cache-buffer "*File Cache*"  
  "Buffer to hold the cache of file names."
  :type 'string
  :group 'file-cache)

(defcustom file-cache-buffer-default-regexp "^.+$"
  "Regexp to match files in `file-cache-buffer'."
  :type 'regexp
  :group 'file-cache)

(defvar file-cache-last-completion nil)

(defvar file-cache-alist nil
  "Internal data structure to hold cache of file names.")

(defvar file-cache-completions-keymap nil
  "Keymap for file cache completions buffer.")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Functions to add files to the cache
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun file-cache-add-directory (directory &optional regexp)
  "Add DIRECTORY to the file cache.
If the optional REGEXP argument is non-nil, only files which match it will 
be added to the cache."
  (interactive "DAdd files from directory: ")
  ;; Not an error, because otherwise we can't use load-paths that
  ;; contain non-existent directories.
  (if (not (file-accessible-directory-p directory))
      (message "Directory %s does not exist" directory)
    (let* ((dir       (expand-file-name directory))
	   (dir-files (directory-files dir t regexp))
	   )
      ;; Filter out files we don't want to see
      (mapcar
       '(lambda (file)
	(mapcar 
	 '(lambda (regexp)
	    (if (string-match regexp file)
		(setq dir-files (delq file dir-files))))
	 file-cache-filter-regexps))
       dir-files)
      (file-cache-add-file-list dir-files))))

(defun file-cache-add-directory-list (directory-list &optional regexp)
  "Add DIRECTORY-LIST (a list of directory names) to the file cache.
If the optional REGEXP argument is non-nil, only files which match it 
will be added to the cache. Note that the REGEXP is applied to the files 
in each directory, not to the directory list itself."
  (interactive "XAdd files from directory list: ")
  (mapcar 
   '(lambda (dir) (file-cache-add-directory dir regexp))
   directory-list))

(defun file-cache-add-file-list  (file-list)
  "Add FILE-LIST (a list of files names) to the file cache."
  (interactive "XFile List: ")
  (mapcar 'file-cache-add-file file-list))

;; Workhorse function
(defun file-cache-add-file (file)
  "Add FILE to the file cache."
  (interactive "fAdd File: ")
  (if (not (file-exists-p file))
      (message "File %s does not exist" file)
    (let* ((file-name (file-name-nondirectory file))
	   (dir-name  (file-name-directory    file))
	   (the-entry (assoc file-name file-cache-alist))
	   )
      ;; Does the entry exist already?
      (if the-entry
	  (if (or (and (stringp (cdr the-entry))
		       (string= dir-name (cdr the-entry)))
		  (and (listp (cdr the-entry))
		       (member dir-name (cdr the-entry))))
	      nil
	    (setcdr the-entry (append (list dir-name) (cdr the-entry)))
	    )
	;; If not, add it to the cache
	(setq file-cache-alist
	      (cons (cons file-name (list dir-name)) 
		    file-cache-alist)))
      )))
  
(defun file-cache-add-directory-using-find (directory)
  "Use the `find' command to add files to the file cache.
Find is run in DIRECTORY."
  (interactive "DAdd files under directory: ")
  (let ((dir (expand-file-name directory)))
    (set-buffer (get-buffer-create file-cache-buffer))
    (erase-buffer)
    (call-process file-cache-find-command nil 
		  (get-buffer file-cache-buffer) nil
		  dir "-name" 
		  (if (memq system-type 
			    (list 'windows-nt 'ms-dos)) "'*'" "*")
		  "-print")
    (file-cache-add-from-file-cache-buffer)))

(defun file-cache-add-directory-using-locate (string)
  "Use the `locate' command to add files to the file cache.
STRING is passed as an argument to the locate command."
  (interactive "sAdd files using locate string: ")
  (set-buffer (get-buffer-create file-cache-buffer))
  (erase-buffer)
  (call-process file-cache-locate-command nil 
		(get-buffer file-cache-buffer) nil
		string)
  (file-cache-add-from-file-cache-buffer))

(defun file-cache-add-from-file-cache-buffer (&optional regexp)
  "Add any entries found in the file cache buffer.
Each entry matches the regular expression `file-cache-buffer-default-regexp'
or the optional REGEXP argument."
  (set-buffer file-cache-buffer)
  (mapcar 
   (function (lambda (elt)
	       (goto-char (point-min))
	       (delete-matching-lines elt)))
   file-cache-filter-regexps)
  (goto-char (point-min))
  (let ((full-filename))
    (while (re-search-forward
	    (or regexp file-cache-buffer-default-regexp) 
	    (point-max) t)
      (setq full-filename (buffer-substring-no-properties
		      (match-beginning 0) (match-end 0))) 
      (file-cache-add-file full-filename))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Functions to delete from the cache
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun file-cache-clear-cache ()
  "Clear the file cache."
  (interactive)
  (setq file-cache-alist nil))

;; This clears *all* files with the given name
(defun file-cache-delete-file (file)
  "Delete FILE from the file cache."
  (interactive
   (list (completing-read "Delete file from cache: " file-cache-alist)))
  (setq file-cache-alist 
	(delq (assoc file file-cache-alist) file-cache-alist)))

(defun file-cache-delete-file-list (file-list)
  "Delete FILE-LIST (a list of files) from the file cache."
  (interactive "XFile List: ")
  (mapcar 'file-cache-delete-file file-list))

(defun file-cache-delete-file-regexp (regexp)
  "Delete files matching REGEXP from the file cache."
  (interactive "sRegexp: ")
  (let ((delete-list))
    (mapcar '(lambda (elt) 
	       (and (string-match regexp (car elt))
		    (setq delete-list (cons (car elt) delete-list))))
	    file-cache-alist)
    (file-cache-delete-file-list delete-list)
    (message "Deleted %d files from file cache" (length delete-list))))

(defun file-cache-delete-directory (directory)
  "Delete DIRECTORY from the file cache."
  (interactive "DDelete directory from file cache: ")
  (let ((dir (expand-file-name directory))
	(result 0))
    (mapcar 
     '(lambda (entry) 
	(if (file-cache-do-delete-directory dir entry)
	    (setq result (1+ result))))
     file-cache-alist)
    (if (zerop result)
	(error "No entries containing %s found in cache" directory)
      (message "Deleted %d entries" result))))

(defun file-cache-do-delete-directory (dir entry)
  (let ((directory-list (cdr entry))
	(directory (file-cache-canonical-directory dir))
	)
    (and (member directory directory-list)
	 (if (equal 1 (length directory-list))
	     (setq file-cache-alist 
		   (delq entry file-cache-alist))
	   (setcdr entry (delete directory directory-list)))
	 )
    ))

(defun file-cache-delete-directory-list (directory-list)
  "Delete DIRECTORY-LIST (a list of directories) from the file cache."
  (interactive "XDirectory List: ")
  (mapcar 'file-cache-delete-directory directory-list))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Utility functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Returns the name of a directory for a file in the cache
(defun file-cache-directory-name  (file)
  (let* ((directory-list (cdr (assoc file file-cache-alist)))
	 (len            (length directory-list))
	 (directory)
	 (num)
	 )
    (if (not (listp directory-list))
	(error "Unknown type in file-cache-alist for key %s" file))
    (cond 
     ;; Single element
     ((eq 1 len)
      (setq directory (elt directory-list 0)))
     ;; No elements
     ((eq 0 len)
      (error "No directory found for key %s" file))
     ;; Multiple elements
     (t
      (let* ((minibuffer-dir (file-name-directory (buffer-string)))
	     (dir-list       (member minibuffer-dir directory-list))
	     )
	(setq directory
	      ;; If the directory is in the list, return the next element
	      ;; Otherwise, return the first element
	      (if dir-list 
		  (or (elt directory-list 
			   (setq num (1+ (- len (length dir-list)))))
		      (elt directory-list (setq num 0)))
		(elt directory-list (setq num 0))))
	)
      )
     )
    ;; If there were multiple directories, set up a minibuffer message
    (setq file-cache-multiple-directory-message
	  (and num (format " [%d of %d]" (1+ num) len)))
    directory))

;; Returns the name of a file in the cache
(defun file-cache-file-name  (file)
  (let ((directory (file-cache-directory-name file)))
    (concat directory file)))
 
;; Return a canonical directory for comparison purposes.
;; Such a directory ends with a forward slash.
(defun file-cache-canonical-directory (dir)
  (let ((directory dir))
    (if (not (char-equal ?/ (string-to-char (substring directory -1))))
	(concat directory "/")
      directory)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Minibuffer functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; The prefix argument works around a bug in the minibuffer completion.
;; The completion function doesn't distinguish between the states:
;; 
;;   "Multiple completions of name" (eg, Makefile, Makefile.in)
;;   "Name available in multiple directories" (/tmp/Makefile, ~me/Makefile)
;;   
;; The default is to do the former; a prefix arg forces the latter.

;;;###autoload
(defun file-cache-minibuffer-complete (arg)
  "Complete a filename in the minibuffer using a preloaded cache.
Filecache does two kinds of substitution: it completes on names in
the cache, and, once it has found a unique name, it cycles through
the directories that the name is available in.  With a prefix argument, 
the name is considered already unique; only the second substitution 
\(directories) is done."
  (interactive "P") 
  (let* 
      (
       (completion-ignore-case nil)
       (case-fold-search       nil)
       (string                 (file-name-nondirectory (buffer-string)))
       (completion-string      (try-completion string file-cache-alist))
       (completion-list)
       (len)
       (file-cache-string)
       )
    (cond 
     ;; If it's the only match, replace the original contents
     ((or arg (eq completion-string t))
      (setq file-cache-string (file-cache-file-name string))
      (if (string= file-cache-string (buffer-string))
	  (file-cache-temp-minibuffer-message file-cache-sole-match-message)
	(erase-buffer)
	(insert-string file-cache-string)
	(if file-cache-multiple-directory-message
	    (file-cache-temp-minibuffer-message 
	     file-cache-multiple-directory-message))
      ))

     ;; If it's the longest match, insert it
     ((stringp completion-string)
      ;; If we've already inserted a unique string, see if the user
      ;; wants to use that one
      (if (and (string= string completion-string)
	       (assoc string file-cache-alist))
	  (if (and (eq last-command this-command)
		   (string= file-cache-last-completion completion-string))
	      (progn 
		(erase-buffer)
		(insert-string (file-cache-file-name completion-string))  
		(setq file-cache-last-completion nil)
		)
	    (file-cache-temp-minibuffer-message file-cache-non-unique-message)
	    (setq file-cache-last-completion string)
	    )
	(setq file-cache-last-completion string)
	(setq completion-list (all-completions string file-cache-alist)
	      len             (length completion-list))
	(if (> len 1)
	    (progn
	      (goto-char (point-max))
	      (insert-string 
	       (substring completion-string (length string)))
	      ;; Add our own setup function to the Completions Buffer
	      (let ((completion-setup-hook
		   (reverse 
		    (append (list 'file-cache-completion-setup-function)
			    completion-setup-hook)))
		    )
		(with-output-to-temp-buffer file-cache-completions-buffer
		  (display-completion-list completion-list))
		)
	      )
	  (setq file-cache-string (file-cache-file-name completion-string))
	  (if (string= file-cache-string (buffer-string))
	      (file-cache-temp-minibuffer-message 
	       file-cache-sole-match-message)
	    (erase-buffer)
	    (insert-string file-cache-string)
	    (if file-cache-multiple-directory-message
		(file-cache-temp-minibuffer-message 
		 file-cache-multiple-directory-message)))
	  )))
          
     ;; No match
     ((eq completion-string nil)
      (file-cache-temp-minibuffer-message file-cache-no-match-message))
     )
))

;; Lifted from "complete.el"
(defun file-cache-temp-minibuffer-message (msg)
  "A Lisp version of `temp_minibuffer_message' from minibuf.c."
  (let ((savemax (point-max)))
    (save-excursion
      (goto-char (point-max))
      (insert msg))
    (let ((inhibit-quit t))
      (sit-for 2)
      (delete-region savemax (point-max))
      (if quit-flag
	  (setq quit-flag nil
		unread-command-events (list 7))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Completion functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun file-cache-completion-setup-function  ()
  (set-buffer file-cache-completions-buffer)

  (if file-cache-completions-keymap
      nil
    (setq file-cache-completions-keymap 
	  (copy-keymap completion-list-mode-map))
    (define-key file-cache-completions-keymap [mouse-2] 
      'file-cache-mouse-choose-completion) 
    (define-key file-cache-completions-keymap "\C-m"    
      'file-cache-choose-completion))

    (use-local-map file-cache-completions-keymap)
    )

(defun file-cache-choose-completion  ()
  "Choose a completion in the `*Completions*' buffer."
  (interactive)
  (let ((completion-no-auto-exit t))
    (choose-completion)
    (select-window (active-minibuffer-window))
    (file-cache-minibuffer-complete nil)
    )
  )

(defun file-cache-mouse-choose-completion  (event)
  "Choose a completion with the mouse."
  (interactive "e")
  (let ((completion-no-auto-exit t))
    (mouse-choose-completion event)
    (select-window (active-minibuffer-window))
    (file-cache-minibuffer-complete nil)
    )
  )

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Debugging functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun file-cache-debug-read-from-minibuffer (file)
  "Debugging function."
  (interactive 
   (list (completing-read "File Cache: " file-cache-alist)))
  (message "%s" (assoc file file-cache-alist))
  )

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Keybindings
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;###autoload (define-key minibuffer-local-completion-map [C-tab] 'file-cache-minibuffer-complete)
;;;###autoload (define-key minibuffer-local-map [C-tab] 'file-cache-minibuffer-complete)
;;;###autoload (define-key minibuffer-local-must-match-map [C-tab] 'file-cache-minibuffer-complete)

(provide 'filecache)

;;; filecache.el ends here
