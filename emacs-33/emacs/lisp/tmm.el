;;; tmm.el --- text mode access to menu-bar

;; Copyright (C) 1994, 1995, 1996 Free Software Foundation, Inc.

;; Author: Ilya Zakharevich <ilya@math.mps.ohio-state.edu>
;; Maintainer: FSF

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

;; To use this package add 

;; (autoload 'tmm-menubar 'tmm "Text mode substitute for menubar" t) 
;; (global-set-key [f10] 'tmm-menubar)
;; to your .emacs file. You can also add your own access to different
;; menus available in Window System Emacs modeling definition after
;; tmm-menubar.

;;; Code:

(require 'electric)

(defgroup tmm nil
  "Text mode access to menu-bar."
  :prefix "tmm-"
  :group 'menu)

;;; The following will be localized, added only to pacify the compiler.
(defvar tmm-short-cuts)
(defvar tmm-old-mb-map nil)
(defvar tmm-old-comp-map)
(defvar tmm-c-prompt)
(defvar tmm-km-list)
(defvar tmm-next-shortcut-digit)
(defvar tmm-table-undef)

;;;###autoload (define-key global-map "\M-`" 'tmm-menubar)
;;;###autoload (define-key global-map [f10] 'tmm-menubar)
;;;###autoload (define-key global-map [menu-bar mouse-1] 'tmm-menubar-mouse)

;;;###autoload
(defun tmm-menubar (&optional x-position)
  "Text-mode emulation of looking and choosing from a menubar.
See the documentation for `tmm-prompt'.
X-POSITION, if non-nil, specifies a horizontal position within the menu bar;
we make that menu bar item (the one at that position) the default choice."
  (interactive)
  (run-hooks 'menu-bar-update-hook)
  ;; Obey menu-bar-final-items; put those items last.
  (let ((menu-bar (tmm-get-keybind [menu-bar]))
	menu-bar-item)
    (let ((list menu-bar-final-items))
      (while list
	(let ((item (car list)))
	  ;; ITEM is the name of an item that we want to put last.
	  ;; Find it in MENU-BAR and move it to the end.
	  (let ((this-one (assq item menu-bar)))
	    (setq menu-bar (append (delq this-one menu-bar)
				   (list this-one)))))
	(setq list (cdr list))))
    (if x-position
	(let ((tail menu-bar)
	      this-one
	      (column 0))
	  (while (and tail (< column x-position))
	    (setq this-one (car tail))
	    (if (and (consp (car tail))
		     (consp (cdr (car tail)))
		     (stringp (nth 1 (car tail))))
		(setq column (+ column
				(length (nth 1 (car tail)))
				1)))
	    (setq tail (cdr tail)))
	  (setq menu-bar-item (car this-one))))
    (tmm-prompt menu-bar nil menu-bar-item)))

;;;###autoload
(defun tmm-menubar-mouse (event)
  "Text-mode emulation of looking and choosing from a menubar.
This command is used when you click the mouse in the menubar
on a console which has no window system but does have a mouse.
See the documentation for `tmm-prompt'."
  (interactive "e")
  (tmm-menubar (car (posn-x-y (event-start event)))))

(defcustom tmm-mid-prompt "==>"
  "*String to insert between shortcut and menu item. 
If nil, there will be no shortcuts. It should not consist only of spaces,
or else the correct item might not be found in the `*Completions*' buffer."
  :type 'string
  :group 'tmm)

(defvar tmm-mb-map nil
  "A place to store minibuffer map.")

(defcustom tmm-completion-prompt 
  "Press PageUp Key to reach this buffer from the minibuffer.
Alternatively, you can use Up/Down keys (or your History keys) to change
the item in the minibuffer, and press RET when you are done, or press the 
marked letters to pick up your choice.  Type C-g or ESC ESC ESC to cancel.
"
  "*Help text to insert on the top of the completion buffer.
To save space, you can set this to nil,
in which case the standard introduction text is deleted too."
  :type '(choice string (const nil))
  :group 'tmm)

(defcustom tmm-shortcut-style '(downcase upcase)
  "*What letters to use as menu shortcuts. 
Must be either one of the symbols `downcase' or `upcase', 
or else a list of the two in the order you prefer."
  :type '(choice (const downcase)
		 (const upcase)
		 (repeat (choice (const downcase) (const upcase))))
  :group 'tmm)

(defcustom tmm-shortcut-words 2
  "*How many successive words to try for shortcuts, nil means all.
If you use only one of `downcase' or `upcase' for `tmm-shortcut-style', 
specify nil for this variable."
  :type '(choice integer (const nil))
  :group 'tmm)

;;;###autoload
(defun tmm-prompt (menu &optional in-popup default-item)
  "Text-mode emulation of calling the bindings in keymap.
Creates a text-mode menu of possible choices.  You can access the elements
in the menu in two ways:
   *)  via history mechanism from minibuffer;
   *)  Or via completion-buffer that is automatically shown.
The last alternative is currently a hack, you cannot use mouse reliably.

MENU is like the MENU argument to `x-popup-menu': either a
keymap or an alist of alists.
DEFAULT-ITEM, if non-nil, specifies an initial default choice.
Its value should be an event that has a binding in MENU."
  ;; If the optional argument IN-POPUP is t,
  ;; then MENU is an alist of elements of the form (STRING . VALUE).
  ;; That is used for recursive calls only.
  (let ((gl-str "Menu bar")  ;; The menu bar itself is not a menu keymap
					; so it doesn't have a name.
	tmm-km-list out history history-len tmm-table-undef tmm-c-prompt
	tmm-old-mb-map tmm-old-comp-map tmm-short-cuts
	chosen-string choice
	(not-menu (not (keymapp menu))))
    (run-hooks 'activate-menubar-hook)
    ;; Compute tmm-km-list from MENU.
    ;; tmm-km-list is an alist of (STRING . MEANING).
    ;; It has no other elements.
    ;; The order of elements in tmm-km-list is the order of the menu bar.
    (mapcar (function (lambda (elt)
			(if (stringp elt)
			    (setq gl-str elt)
			  (and (listp elt) (tmm-get-keymap elt not-menu)))))
	    menu)
    ;; Choose an element of tmm-km-list; put it in choice.
    (if (and not-menu (= 1 (length tmm-km-list)))
	;; If this is the top-level of an x-popup-menu menu,
	;; and there is just one pane, choose that one silently.
	;; This way we only ask the user one question,
	;; for which element of that pane.
	(setq choice (cdr (car tmm-km-list)))
      (unless tmm-km-list
	(error "Empty menu reached"))
      (and tmm-km-list
	   (let ((index-of-default 0))
	     (if tmm-mid-prompt
		 (setq tmm-km-list (tmm-add-shortcuts tmm-km-list))
	       t)
	     ;; Find the default item's index within the menu bar.
	     ;; We use this to decide the initial minibuffer contents
	     ;; and initial history position.
	     (if default-item
		 (let ((tail menu))
		   (while (and tail
			       (not (eq (car-safe (car tail)) default-item)))
		     ;; Be careful to count only the elements of MENU
		     ;; that actually constitute menu bar items.
		     (if (and (consp (car tail))
			      (stringp (car-safe (cdr (car tail)))))
			 (setq index-of-default (1+ index-of-default)))
		     (setq tail (cdr tail)))))
	     (setq history (reverse (mapcar 'car tmm-km-list)))
	     (setq history-len (length history))
	     (setq history (append history history history history))
	     (setq tmm-c-prompt (nth (- history-len 1 index-of-default) history))
	     (add-hook 'minibuffer-setup-hook 'tmm-add-prompt)
	     (save-excursion
	       (unwind-protect
		   (setq out
			 (completing-read
			  (concat gl-str " (up/down to change, PgUp to menu): ")
			  tmm-km-list nil t nil
			  (cons 'history (- (* 2 history-len) index-of-default))))
		 (save-excursion
		   (remove-hook 'minibuffer-setup-hook 'tmm-add-prompt)
		   (if (get-buffer "*Completions*")
		       (progn
			 (set-buffer "*Completions*")
			 (use-local-map tmm-old-comp-map)
			 (bury-buffer (current-buffer)))))
		 ))))
      (setq choice (cdr (assoc out tmm-km-list)))
      (and (null choice)
	   (> (length out) (length tmm-c-prompt))
	   (string= (substring out 0 (length tmm-c-prompt)) tmm-c-prompt)
	   (setq out (substring out (length tmm-c-prompt))
		 choice (cdr (assoc out tmm-km-list))))
      (and (null choice) out
	   (setq out (try-completion out tmm-km-list)
		 choice (cdr (assoc  out tmm-km-list)))))
    ;; CHOICE is now (STRING . MEANING).  Separate the two parts.
    (setq chosen-string (car choice))
    (setq choice (cdr choice))
    (cond (in-popup
	   ;; We just did the inner level of a -popup menu.
	   choice)
	  ;; We just did the outer level.  Do the inner level now.
	  (not-menu (tmm-prompt choice t)) 
	  ;; We just handled a menu keymap and found another keymap.
	  ((keymapp choice)
	   (if (symbolp choice)
	       (setq choice (indirect-function choice)))
	   (condition-case nil
	       (require 'mouse)
	     (error nil))
	   (condition-case nil
	       (x-popup-menu nil choice) ; Get the shortcuts
	     (error nil))
	   (tmm-prompt choice))
	  ;; We just handled a menu keymap and found a command.
	  (choice
	   (if chosen-string
	       (progn
		 (setq last-command-event chosen-string)
		 (call-interactively choice))
	     choice)))))

(defun tmm-add-shortcuts (list)
  "Adds shortcuts to cars of elements of the list.
Takes a list of lists with a string as car, returns list with
shortcuts added to these cars.
Stores a list of all the shortcuts in the free variable `tmm-short-cuts'."
  (let ((tmm-next-shortcut-digit ?0))
    (mapcar 'tmm-add-one-shortcut (reverse list))))

(defsubst tmm-add-one-shortcut (elt)
;; uses the free vars tmm-next-shortcut-digit and tmm-short-cuts
  (let* ((str (car elt))
        (paren (string-match "(" str))  
        (pos 0) (word 0) char)
    (catch 'done                        ; ??? is this slow?
      (while (and (or (not tmm-shortcut-words) ; no limit on words
                      (< word tmm-shortcut-words)) ; try n words
                  (setq pos (string-match "\\w+" str pos)) ; get next word
                  (not (and paren (> pos paren)))) ; don't go past "(binding.."
        (if (or (= pos 0)
                (/= (aref str (1- pos)) ?.)) ; avoid file extensions
            (let ((shortcut-style                 
                   (if (listp tmm-shortcut-style) ; convert to list
                       tmm-shortcut-style
                     (list tmm-shortcut-style))))
              (while shortcut-style     ; try upcase and downcase variants
                (setq char (funcall (car shortcut-style) (aref str pos)))
                (if (not (memq char tmm-short-cuts)) (throw 'done char))
                (setq shortcut-style (cdr shortcut-style)))))
        (setq word (1+ word))
        (setq pos (match-end 0)))
      (while (<= tmm-next-shortcut-digit ?9) ; no letter shortcut, pick a digit
        (setq char tmm-next-shortcut-digit)
        (setq tmm-next-shortcut-digit (1+ tmm-next-shortcut-digit))
        (if (not (memq char tmm-short-cuts)) (throw 'done char)))
      (setq char nil))
    (if char (setq tmm-short-cuts (cons char tmm-short-cuts)))
    (cons (concat (if char (concat (char-to-string char) tmm-mid-prompt)
                    ;; keep them lined up in columns
                    (make-string (1+ (length tmm-mid-prompt)) ?\ ))
                  str)
          (cdr elt))))

;; This returns the old map.
(defun tmm-define-keys (minibuffer)
  (let ((map (make-sparse-keymap)))
    (suppress-keymap map t)
    (mapcar
     (function
      (lambda (c)
        (if (listp tmm-shortcut-style)
            (define-key map (char-to-string c) 'tmm-shortcut)
          ;; only one kind of letters are shortcuts, so map both upcase and
          ;; downcase input to the same
          (define-key map (char-to-string (downcase c)) 'tmm-shortcut)
          (define-key map (char-to-string (upcase c)) 'tmm-shortcut))))
     tmm-short-cuts)
    (if minibuffer
	(progn
          (define-key map [pageup] 'tmm-goto-completions)
          (define-key map [prior] 'tmm-goto-completions)
          (define-key map "\ev" 'tmm-goto-completions)
          (define-key map "\C-n" 'next-history-element)
          (define-key map "\C-p" 'previous-history-element)))
    (prog1 (current-local-map)
      (use-local-map (append map (current-local-map))))))

(defun tmm-completion-delete-prompt ()
  (set-buffer standard-output)
  (goto-char 1)
  (delete-region 1 (search-forward "Possible completions are:\n")))

(defun tmm-add-prompt ()
  (remove-hook 'minibuffer-setup-hook 'tmm-add-prompt)
  (make-local-hook 'minibuffer-exit-hook)
  (add-hook 'minibuffer-exit-hook 'tmm-delete-map nil t)
  (let ((win (selected-window)))
    (setq tmm-old-mb-map (tmm-define-keys t))
    ;; Get window and hide it for electric mode to get correct size
    (save-window-excursion 
      (let ((completions
	     (mapcar 'car minibuffer-completion-table)))
        (or tmm-completion-prompt
            (add-hook 'completion-setup-hook
                      'tmm-completion-delete-prompt 'append))
	(with-output-to-temp-buffer "*Completions*"
	  (display-completion-list completions))
        (remove-hook 'completion-setup-hook 'tmm-completion-delete-prompt))
      (if tmm-completion-prompt
          (progn
	    (set-buffer "*Completions*")
	    (goto-char 1)
            (insert tmm-completion-prompt)))
      )
    (save-selected-window
      (other-window 1)			; Electric-pop-up-window does
					; not work in minibuffer
      (Electric-pop-up-window "*Completions*")
      (with-current-buffer "*Completions*"
	(setq tmm-old-comp-map (tmm-define-keys nil))))

    (insert tmm-c-prompt)))

(defun tmm-delete-map ()
  (remove-hook 'minibuffer-exit-hook 'tmm-delete-map t)
  (if tmm-old-mb-map
      (use-local-map tmm-old-mb-map)))

(defun tmm-shortcut ()
  "Choose the shortcut that the user typed."
  (interactive)
  (let ((c last-command-char) s)
    (if (symbolp tmm-shortcut-style)
        (setq c (funcall tmm-shortcut-style c)))
    (if (memq c tmm-short-cuts)
	(if (equal (buffer-name) "*Completions*")
	    (progn
	      (beginning-of-buffer)
	      (re-search-forward
	       (concat "\\(^\\|[ \t]\\)" (char-to-string c) tmm-mid-prompt))
	      (choose-completion))
	  (erase-buffer)		; In minibuffer
	  (mapcar (lambda (elt)
		    (if (string=
			 (substring (car elt) 0 
				    (min (1+ (length tmm-mid-prompt))
					 (length (car elt))))
			 (concat (char-to-string c) tmm-mid-prompt))
			(setq s (car elt))))
		  tmm-km-list)
	  (insert s)
	  (exit-minibuffer)))))

(defun tmm-goto-completions ()
  (interactive)
  (setq tmm-c-prompt (buffer-string))
  (erase-buffer)
  (switch-to-buffer-other-window "*Completions*")
  (search-forward tmm-c-prompt)
  (search-backward tmm-c-prompt))

(defun tmm-get-keymap (elt &optional in-x-menu) 
  "Prepends (DOCSTRING EVENT BINDING) to free variable `tmm-km-list'.
The values are deduced from the argument ELT, that should be an
element of keymap, an `x-popup-menu' argument, or an element of
`x-popup-menu' argument (when IN-X-MENU is not-nil).
This function adds the element only if it is not already present.
It uses the free variable `tmm-table-undef' to keep undefined keys."
  (let (km str cache plist filter (event (car elt)))
    (setq elt (cdr elt))
    (if (eq elt 'undefined)
	(setq tmm-table-undef (cons (cons event nil) tmm-table-undef))
      (unless (assoc event tmm-table-undef)
	(cond ((if (listp elt)
		   (or (keymapp elt) (eq (car elt) 'lambda))
		 (fboundp elt))
	       (setq km elt))
	      ((if (listp (cdr-safe elt))
		   (or (keymapp (cdr-safe elt))
		       (eq (car (cdr-safe elt)) 'lambda))
		 (fboundp (cdr-safe elt)))
	       (setq km (cdr elt))
	       (and (stringp (car elt)) (setq str (car elt))))
	      ((if (listp (cdr-safe (cdr-safe elt)))
		   (or (keymapp (cdr-safe (cdr-safe elt)))
		       (eq (car (cdr-safe (cdr-safe elt))) 'lambda))
		 (fboundp (cdr-safe (cdr-safe elt))))
	       (setq km (cdr (cdr elt)))
	       (and (stringp (car elt)) (setq str (car elt)))
	       (and str
		    (stringp (cdr (car (cdr elt)))) ; keyseq cache
		    (setq cache (cdr (car (cdr elt))))
		    cache (setq str (concat str cache))))
	      ((eq (car-safe elt) 'menu-item)
	       (setq plist (cdr-safe (cdr-safe (cdr-safe elt))))
	       (setq km (nth 2 elt))
	       (setq str (nth 1 elt))
	       (setq filter (plist-get plist :filter))
	       (if filter
		   (setq km (funcall filter km)))
	       (and str
		    (consp (nth 3 elt))
		    (stringp (cdr (nth 3 elt))) ; keyseq cache
		    (setq cache (cdr (nth 3 elt)))
		    cache
		    (setq str (concat str cache))))
	      ((if (listp (cdr-safe (cdr-safe (cdr-safe elt))))
		   (or (keymapp (cdr-safe (cdr-safe (cdr-safe elt))))
		       (eq (car (cdr-safe (cdr-safe (cdr-safe elt)))) 'lambda))
		 (fboundp (cdr-safe (cdr-safe (cdr-safe elt)))))
					 ; New style of easy-menu
	       (setq km (cdr (cdr (cdr elt))))
	       (and (stringp (car elt)) (setq str (car elt)))
	       (and str
		    (stringp (cdr (car (cdr (cdr elt))))) ; keyseq cache
		    (setq cache (cdr (car (cdr (cdr elt)))))
		    cache (setq str (concat str cache))))
	      ((stringp event)		; x-popup or x-popup element
	       (if (or in-x-menu (stringp (car-safe elt)))
		   (setq str event event nil km elt)
		 (setq str event event nil km (cons 'keymap elt))
		 ))))
      (and km (stringp km) (setq str km))
      ;; Verify that the command is enabled;
      ;; if not, don't mention it.
      (when (and km (symbolp km) (get km 'menu-enable))
	(unless (eval (get km 'menu-enable))
	  (setq km nil)))
      (and km str
	   (or (assoc str tmm-km-list)
	       (setq tmm-km-list 
		     (cons (cons str (cons event km)) tmm-km-list)))
	   ))))

(defun tmm-get-keybind (keyseq)
  "Return the current binding of KEYSEQ, merging prefix definitions.
If KEYSEQ is a prefix key that has local and global bindings,
we merge them into a single keymap which shows the proper order of the menu.
However, for the menu bar itself, the value does not take account
of `menu-bar-final-items'."
  (let (allbind bind)
    (setq bind (key-binding keyseq))
    ;; If KEYSEQ is a prefix key, then BIND is either nil
    ;; or a symbol defined as a keymap (which satisfies keymapp).
    (if (keymapp bind)
	(setq bind nil))
    ;; If we have a non-keymap definition, return that.
    (or bind
	(progn
	  ;; Otherwise, it is a prefix, so make a list of the subcommands.
	  ;; Make a list of all the bindings in all the keymaps.
	  (setq allbind (mapcar 'cdr (minor-mode-key-binding keyseq))) 
	  (setq allbind (cons (local-key-binding keyseq) allbind))
	  (setq allbind (cons (global-key-binding keyseq) allbind))
	  ;; Merge all the elements of ALLBIND into one keymap.
	  (mapcar (lambda (in)
		    (if (and (symbolp in) (keymapp in))
			(setq in (symbol-function in)))
		    (and in (keymapp in)
			 (if (keymapp bind)
			     (setq bind (nconc bind (copy-sequence (cdr in))))
			   (setq bind (copy-sequence in)))))
		  allbind)
	  ;; Return that keymap.
	  bind))))

(add-hook 'calendar-load-hook (lambda () (require 'cal-menu)))

(provide 'tmm)

;;; tmm.el ends here
