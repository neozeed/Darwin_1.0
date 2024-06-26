;; crisp.el --- CRiSP/Brief Emacs emulator

;; Copyright (C) 1997, 1998 Free Software Foundation, Inc.

;; Author: Gary D. Foster <gfoster@suzieq.ml.org>
;; Keywords: emulations brief crisp

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

;; Keybindings and minor functions to duplicate the functionality and
;; finger-feel of the CRiSP/Brief editor.  This package is designed to
;; facilitate transitioning from Brief to (XE|E)macs with a minimum
;; amount of hassles.

;; Enable this package by putting (require 'crisp) in your .emacs and
;; use M-x crisp-mode to toggle it on or off.

;; This package will automatically load the scroll-all.el package if
;; you put (setq crisp-load-scroll-all t) in your .emacs before
;; loading this package.  If this feature is enabled, it will bind
;; meta-f1 to the scroll-all mode toggle.  The scroll-all package
;; duplicates the scroll-alling feature in CRiSP.

;; Also, the default keybindings for brief/CRiSP override the M-x
;; key to exit the editor.  If you don't like this functionality, you
;; can prevent this behavior (or redefine it dynamically) by setting
;; the value of `crisp-override-meta-x' either in your .emacs or
;; interactively.  The default setting is nil, which means that M-x will
;; by default run `execute-extended-command' instead of the command
;; `save-buffers-kill-emacs'.

;; Finally, if you want to change the string displayed in the modeline
;; when this mode is in effect, override the definition of
;; `crisp-mode-modeline-string' in your .emacs.  The default value is
;; " *Crisp*" which may be a bit lengthy if you have a lot of things
;; being displayed there.

;; All these overrides should go *before* the (require 'crisp) statement.

;; Code:

;; local variables

(defgroup crisp nil
  "Emulator for CRiSP and Brief key bindings."
  :prefix "crisp-"
  :group 'emulations)

(defvar crisp-mode-map (let ((map (make-sparse-keymap)))
			 (set-keymap-parent map (current-global-map))
			 map)
  "Local keymap for CRiSP emulation mode.
All the bindings are done here instead of globally to try and be
nice to the world.")

(defcustom crisp-mode-modeline-string " *CRiSP*"
  "*String to display in the modeline when CRiSP emulation mode is enabled."
  :type 'string
  :group 'crisp)

(defvar crisp-mode-original-keymap (current-global-map)
  "The original keymap before CRiSP emulation mode remaps anything.
This keymap is restored when CRiSP emulation mode is disabled.")

(defcustom crisp-mode-enabled nil
  "Track status of CRiSP emulation mode.
A value of nil means CRiSP mode is not enabled.  A value of t
indicates CRiSP mode is enabled."
  :type 'boolean
  :group 'crisp)

(defcustom crisp-override-meta-x t
  "*Controls overriding the normal Emacs M-x key binding in the CRiSP emulator.
Normally the CRiSP emulator rebinds M-x to `save-buffers-exit-emacs', and
provides the usual M-x functionality on the F10 key.  If this variable
is non-nil, M-x will exit Emacs."
  :type 'boolean
  :group 'crisp)

(defcustom crisp-load-scroll-all nil
  "Controls loading of the Scroll Lock in the CRiSP emulator.
Its default behavior is to load and enable the Scroll Lock minor mode
package when enabling the CRiSP emulator.

If this variable is nil when you start the CRiSP emulator, it
does not load the scroll-all package."
  :type 'boolean
  :group 'crisp)

(defcustom crisp-load-hook nil
  "Hooks to run after loading the CRiSP emulator package."
  :type 'hook
  :group 'crisp)

(defconst crisp-version "1.33"
  "The version of the CRiSP emulator.")

(defconst crisp-mode-help-address "gfoster@suzieq.ml.org"
  "The email address of the CRiSP mode author/maintainer.")

;; Silence the byte-compiler.
(defvar crisp-last-last-command nil
  "The previous value of `last-command'.")

;; The cut and paste routines are different between XEmacs and Emacs
;; so we need to set up aliases for the functions.

(defalias 'crisp-set-clipboard
  (if (fboundp 'clipboard-kill-ring-save)
      'clipboard-kill-ring-save
    'copy-primary-selection))

(defalias 'crisp-kill-region
  (if (fboundp 'clipboard-kill-region)
      'clipboard-kill-region
    'kill-primary-selection))

(defalias 'crisp-yank-clipboard
  (if (fboundp 'clipboard-yank)
      'clipboard-yank
    'yank-clipboard-selection))

;; force transient-mark-mode in Emacs, so that the marking routines
;; work as expected.  If the user turns off transient mark mode,
;; most things will still work fine except the crisp-(copy|kill)
;; functions won't work quite as nicely when regions are marked
;; differently and could really confuse people.  Caveat emptor.

(if (fboundp 'transient-mark-mode)
    (transient-mark-mode t))

(defun crisp-region-active ()
  "Compatibility function to test for an active region."
  (if (boundp 'zmacs-region-active-p)
      zmacs-region-active-p
    mark-active))

;; and now the keymap defines

(define-key crisp-mode-map [(f1)]           'other-window)

(define-key crisp-mode-map [(f2) (down)]    'enlarge-window)
(define-key crisp-mode-map [(f2) (left)]    'shrink-window-horizontally)
(define-key crisp-mode-map [(f2) (right)]   'enlarge-window-horizontally)
(define-key crisp-mode-map [(f2) (up)]      'shrink-window)
(define-key crisp-mode-map [(f3) (down)]    'split-window-vertically)
(define-key crisp-mode-map [(f3) (right)]   'split-window-horizontally)

(define-key crisp-mode-map [(f4)]           'delete-window)
(define-key crisp-mode-map [(control f4)]   'delete-other-windows)

(define-key crisp-mode-map [(f5)]           'search-forward-regexp)
(define-key crisp-mode-map [(f19)]          'search-forward-regexp)
(define-key crisp-mode-map [(meta f5)]      'search-backward-regexp)

(define-key crisp-mode-map [(f6)]           'query-replace)

(define-key crisp-mode-map [(f7)]           'start-kbd-macro)
(define-key crisp-mode-map [(meta f7)]      'end-kbd-macro)

(define-key crisp-mode-map [(f8)]           'call-last-kbd-macro)
(define-key crisp-mode-map [(meta f8)]      'save-kbd-macro)

(define-key crisp-mode-map [(f9)]           'find-file)
(define-key crisp-mode-map [(meta f9)]      'load-library)

(define-key crisp-mode-map [(f10)]          'execute-extended-command)
(define-key crisp-mode-map [(meta f10)]     'compile)

(define-key crisp-mode-map [(SunF37)]       'kill-buffer)
(define-key crisp-mode-map [(kp-add)]       'crisp-copy-line)
(define-key crisp-mode-map [(kp-subtract)]  'crisp-kill-line)
;; just to cover all the bases (GNU Emacs, for instance)
(define-key crisp-mode-map [(f24)]          'crisp-kill-line)
(define-key crisp-mode-map [(insert)]       'crisp-yank-clipboard)
(define-key crisp-mode-map [(f16)]          'crisp-set-clipboard) ; copy on Sun5 kbd
(define-key crisp-mode-map [(f20)]          'crisp-kill-region) ; cut on Sun5 kbd 
(define-key crisp-mode-map [(f18)]          'crisp-yank-clipboard) ; paste on Sun5 kbd

(define-key crisp-mode-map [(control f)]    'fill-paragraph-or-region)
(define-key crisp-mode-map [(meta d)]       (lambda ()
					      (interactive)
					      (beginning-of-line) (kill-line)))
(define-key crisp-mode-map [(meta e)]       'find-file)
(define-key crisp-mode-map [(meta g)]       'goto-line)
(define-key crisp-mode-map [(meta h)]       'help)
(define-key crisp-mode-map [(meta i)]       'overwrite-mode)
(define-key crisp-mode-map [(meta j)]       'bookmark-jump)
(define-key crisp-mode-map [(meta l)]       'crisp-mark-line)
(define-key crisp-mode-map [(meta m)]       'set-mark-command)
(define-key crisp-mode-map [(meta n)]       'bury-buffer)
(define-key crisp-mode-map [(meta p)]       'crisp-unbury-buffer)
(define-key crisp-mode-map [(meta u)]       'advertised-undo)
(define-key crisp-mode-map [(f14)]          'advertised-undo)
(define-key crisp-mode-map [(meta w)]       'save-buffer)
(define-key crisp-mode-map [(meta x)]       'crisp-meta-x-wrapper)
(define-key crisp-mode-map [(meta ?0)]      (lambda ()
					      (interactive)
					      (bookmark-set "0")))
(define-key crisp-mode-map [(meta ?1)]      (lambda ()
					      (interactive)
					      (bookmark-set "1")))
(define-key crisp-mode-map [(meta ?2)]      (lambda ()
					      (interactive)
					      (bookmark-set "2")))
(define-key crisp-mode-map [(meta ?3)]      (lambda ()
					      (interactive)
					      (bookmark-set "3")))
(define-key crisp-mode-map [(meta ?4)]      (lambda ()
					      (interactive)
					      (bookmark-set "4")))
(define-key crisp-mode-map [(meta ?5)]      (lambda ()
					      (interactive)
					      (bookmark-set "5")))
(define-key crisp-mode-map [(meta ?6)]      (lambda ()
					      (interactive)
					      (bookmark-set "6")))
(define-key crisp-mode-map [(meta ?7)]      (lambda ()
					      (interactive)
					      (bookmark-set "7")))
(define-key crisp-mode-map [(meta ?8)]      (lambda ()
					      (interactive)
					      (bookmark-set "8")))
(define-key crisp-mode-map [(meta ?9)]      (lambda ()
					      (interactive)
					      (bookmark-set "9")))

(define-key crisp-mode-map [(shift delete)]    'kill-word)
(define-key crisp-mode-map [(shift backspace)] 'backward-kill-word)
(define-key crisp-mode-map [(control left)]    'backward-word)
(define-key crisp-mode-map [(control right)]   'forward-word)

(define-key crisp-mode-map [(home)]            'crisp-home)
(define-key crisp-mode-map [(control home)]    (lambda ()
						 (interactive)
						 (move-to-window-line 0)))
(define-key crisp-mode-map [(meta home)]       'beginning-of-line)
(define-key crisp-mode-map [(end)]             'crisp-end)
(define-key crisp-mode-map [(control end)]     (lambda ()
						 (interactive)
						 (move-to-window-line -1)))
(define-key crisp-mode-map [(meta end)]        'end-of-line)

(define-key crisp-mode-map [(control c) (b)]   'crisp-submit-bug-report)

(defun crisp-version (&optional arg)
  "Version number of the CRiSP emulator package.
If ARG, insert results at point."
  (interactive "P")
  (let ((foo (concat "CRiSP version " crisp-version)))
    (if arg
	(insert (message foo))
      (message foo))))

(defun crisp-mark-line (arg)
  "Set mark at the end of the line.  Arg works as in `end-of-line'."
  (interactive "p")
  (let (newmark)
    (save-excursion
      (end-of-line arg)
      (setq newmark (point)))
    (push-mark newmark nil t)))

(defun crisp-kill-line (arg)
  "Mark and kill line(s).
Marks from point to end of the current line (honoring prefix arguments),
copies the region to the kill ring and clipboard, and then deletes it."
  (interactive "*p")
  (if (crisp-region-active)
      (call-interactively 'crisp-kill-region)
    (crisp-mark-line arg)
    (call-interactively 'crisp-kill-region)))

(defun crisp-copy-line (arg)
  "Mark and copy line(s).
Marks from point to end of the current line (honoring prefix arguments),
copies the region to the kill ring and clipboard, and then deactivates
the region."
  (interactive "*p")
    (if (crisp-region-active)
	(call-interactively 'crisp-set-clipboard)
      (crisp-mark-line arg)
      (call-interactively 'crisp-set-clipboard))
    ;; clear the region after the operation is complete
    ;; XEmacs does this automagically, Emacs doesn't.
    (if (boundp 'mark-active)
	(setq mark-active nil)))

(defun crisp-home ()
  "\"Home\" the point, the way CRiSP would do it.
The first use moves point to beginning of the line.  Second
consecutive use moves point to beginning of the screen.  Third
consecutive use moves point to the beginning of the buffer."
  (interactive nil)
  (cond
    ((and (eq last-command 'crisp-home)
	  (eq crisp-last-last-command 'crisp-home))
     (goto-char (point-min)))
    ((eq last-command 'crisp-home)
     (move-to-window-line 0))
    (t
     (beginning-of-line)))
  (setq crisp-last-last-command last-command))

(defun crisp-end ()
  "\"End\" the point, the way CRiSP would do it.
The first use moves point to end of the line.  Second
consecutive use moves point to the end of the screen.  Third
consecutive use moves point to the end of the buffer."
  (interactive nil)
  (cond
    ((and (eq last-command 'crisp-end)
	  (eq crisp-last-last-command 'crisp-end))
     (goto-char (point-max)))
    ((eq last-command 'crisp-end)
     (move-to-window-line -1)
     (end-of-line))
    (t
     (end-of-line)))
  (setq crisp-last-last-command last-command))

(defun crisp-unbury-buffer ()
  "Go back one buffer"
  (interactive)
  (switch-to-buffer (car (last (buffer-list)))))
 
(defun crisp-meta-x-wrapper ()
  "Wrapper function to conditionally override the normal M-x bindings.
When `crisp-override-meta-x' is non-nil, M-x will exit Emacs (the
normal CRiSP binding) and when it is nil M-x will run
`execute-extended-command' (the normal Emacs binding)."
  (interactive)
  (if crisp-override-meta-x
      (save-buffers-kill-emacs)
    (call-interactively 'execute-extended-command)))

;; bug reporter

(defun crisp-submit-bug-report ()
  "Submit via mail a bug report on CRiSP Mode."
  (interactive)
  ;; load in reporter
  (let ((reporter-prompt-for-summary-p t)
	(reporter-dont-compact-list '(c-offsets-alist)))
    (and
     (if (y-or-n-p "Do you want to submit a report on CRiSP Mode? ")
	 t (message "") nil)
     (require 'reporter)
     (reporter-submit-bug-report
      crisp-mode-help-address
      (concat "CRiSP Mode [" crisp-version "]")
      nil
      nil
      nil
      "Dear Gary,"
      ))))

;; Now enable the mode

(defun crisp-mode (&optional arg)
  "Toggle CRiSP emulation minor mode.
With ARG, turn CRiSP mode on if ARG is positive, off otherwise."
  (interactive "P")
  (setq crisp-mode-enabled (if (null arg)
			       (not crisp-mode-enabled)
			     (> (prefix-numeric-value arg) 0)))
  (cond
   ((eq crisp-mode-enabled 't)
    (use-global-map crisp-mode-map)
    (if crisp-load-scroll-all
	(require 'scroll-all))
    (if (featurep 'scroll-all)
	(define-key crisp-mode-map [(meta f1)] 'scroll-all-mode))
    (run-hooks 'crisp-load-hook))
   ((eq crisp-mode-enabled 'nil)
    (use-global-map crisp-mode-original-keymap))))

(if (fboundp 'add-minor-mode)
    (add-minor-mode 'crisp-mode-enabled 'crisp-mode-modeline-string
		    nil nil 'crisp-mode)
  (or (assq 'crisp-mode-enabled minor-mode-alist)
      (setq minor-mode-alist
	    (cons '(crisp-mode-enabled crisp-mode-modeline-string) minor-mode-alist))))

(provide 'crisp)

;;; crisp.el ends here
