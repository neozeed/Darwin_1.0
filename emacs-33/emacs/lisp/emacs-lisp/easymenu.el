;;; easymenu.el --- support the easymenu interface for defining a menu.

;; Copyright (C) 1994, 1996, 1998 Free Software Foundation, Inc.

;; Keywords: emulations
;; Author: rms

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

;; This is compatible with easymenu.el by Per Abrahamsen
;; but it is much simpler as it doesn't try to support other Emacs versions.
;; The code was mostly derived from lmenu.el.

;;; Code:

(defcustom easy-menu-precalculate-equivalent-keybindings t
  "Determine when equivalent key bindings are computed for easy-menu menus.
It can take some time to calculate the equivalent key bindings that are shown
in a menu.  If the variable is on, then this calculation gives a (maybe
noticeable) delay when a mode is first entered.  If the variable is off, then
this delay will come when a menu is displayed the first time.  If you never use
menus, turn this variable off, otherwise it is probably better to keep it on."
  :type 'boolean
  :group 'menu
  :version "20.3")

;;;###autoload
(defmacro easy-menu-define (symbol maps doc menu)
  "Define a menu bar submenu in maps MAPS, according to MENU.
The menu keymap is stored in symbol SYMBOL, both as its value
and as its function definition.   DOC is used as the doc string for SYMBOL.

The first element of MENU must be a string.  It is the menu bar item name.
It may be followed by the keyword argument pair
   :filter FUNCTION
FUNCTION is a function with one argument, the menu.  It returns the actual
menu displayed.

The rest of the elements are menu items.

A menu item is usually a vector of three elements:  [NAME CALLBACK ENABLE]

NAME is a string--the menu item name.

CALLBACK is a command to run when the item is chosen,
or a list to evaluate when the item is chosen.

ENABLE is an expression; the item is enabled for selection
whenever this expression's value is non-nil.

Alternatively, a menu item may have the form: 

   [ NAME CALLBACK [ KEYWORD ARG ] ... ]

Where KEYWORD is one of the symbols defined below.

   :keys KEYS

KEYS is a string; a complex keyboard equivalent to this menu item.
This is normally not needed because keyboard equivalents are usually
computed automatically.

   :active ENABLE

ENABLE is an expression; the item is enabled for selection
whenever this expression's value is non-nil.

   :suffix NAME

NAME is a string; the name of an argument to CALLBACK.

   :style STYLE
   
STYLE is a symbol describing the type of menu item.  The following are
defined:  

toggle: A checkbox.
        Prepend the name with '(*) ' or '( ) ' depending on if selected or not.
radio: A radio button.
       Prepend the name with '[X] ' or '[ ] ' depending on if selected or not.
nil: An ordinary menu item.

   :selected SELECTED

SELECTED is an expression; the checkbox or radio button is selected
whenever this expression's value is non-nil.

A menu item can be a string.  Then that string appears in the menu as
unselectable text.  A string consisting solely of hyphens is displayed
as a solid horizontal line.

A menu item can be a list.  It is treated as a submenu.
The first element should be the submenu name.  That's used as the
menu item name in the top-level menu.  It may be followed by the :filter
FUNCTION keyword argument pair.  The rest of the submenu list are menu items,
as above."
  `(progn
     (defvar ,symbol nil ,doc)
     (easy-menu-do-define (quote ,symbol) ,maps ,doc ,menu)))

;;;###autoload
(defun easy-menu-do-define (symbol maps doc menu)
  ;; We can't do anything that might differ between Emacs dialects in
  ;; `easy-menu-define' in order to make byte compiled files
  ;; compatible.  Therefore everything interesting is done in this
  ;; function. 
  (set symbol (easy-menu-create-menu (car menu) (cdr menu)))
  (fset symbol (` (lambda (event) (, doc) (interactive "@e")
		    (x-popup-menu event (, symbol)))))
  (mapcar (function (lambda (map) 
	    (define-key map (vector 'menu-bar (intern (car menu)))
	      (cons (car menu) (symbol-value symbol)))))
	  (if (keymapp maps) (list maps) maps)))

(defun easy-menu-filter-return (menu)
 "Convert MENU to the right thing to return from a menu filter.
MENU is a menu as computed by `easy-menu-define' or `easy-menu-create-menu' or
a symbol whose value is such a menu.
In Emacs a menu filter must return a menu (a keymap), in XEmacs a filter must
return a menu items list (without menu name and keywords). This function
returns the right thing in the two cases."
 (easy-menu-get-map menu nil))		; Get past indirections.

;;;###autoload
(defun easy-menu-create-menu (menu-name menu-items)
  "Create a menu called MENU-NAME with items described in MENU-ITEMS.
MENU-NAME is a string, the name of the menu.  MENU-ITEMS is a list of items
possibly preceded by keyword pairs as described in `easy-menu-define'."
  (let ((menu (make-sparse-keymap menu-name))
	prop keyword arg label enable filter visible)
    ;; Look for keywords.
    (while (and menu-items (cdr menu-items)
		(symbolp (setq keyword (car menu-items)))
		(= ?: (aref (symbol-name keyword) 0)))
      (setq arg (cadr menu-items))
      (setq menu-items (cddr menu-items))
      (cond
       ((eq keyword ':filter) (setq filter arg))
       ((eq keyword ':active) (setq enable (or arg ''nil)))
       ((eq keyword ':label) (setq label arg))
       ((eq keyword ':visible) (setq visible (or arg ''nil)))))
    (if (equal visible ''nil) nil	; Invisible menu entry, return nil.
      (if (and visible (not (easy-menu-always-true visible)))
	  (setq prop (cons :visible (cons visible prop))))
      (if (and enable (not (easy-menu-always-true enable)))
	  (setq prop (cons :enable (cons enable prop))))
      (if filter (setq prop (cons :filter (cons filter prop))))
      (if label (setq prop (cons nil (cons label prop))))
      (while menu-items
	(easy-menu-do-add-item menu (car menu-items))
	(setq menu-items (cdr menu-items)))
      (when prop
	(setq menu (easy-menu-make-symbol menu))
	(put menu 'menu-prop prop))
      menu)))


;; Button prefixes.
(defvar easy-menu-button-prefix
  '((radio . :radio) (toggle . :toggle)))

(defun easy-menu-do-add-item (menu item &optional before)
  ;; Parse an item description and add the item to a keymap.  This is
  ;; the function that is used for item definition by the other easy-menu
  ;; functions.
  ;; MENU is a sparse keymap i.e. a list starting with the symbol `keymap'.
  ;; ITEM defines an item as in `easy-menu-define'.
  ;; Optional argument BEFORE is nil or a key in MENU.  If BEFORE is not nil
  ;; put item before BEFORE in MENU, otherwise if item is already present in
  ;; MENU, just change it, otherwise put it last in MENU.
  (let (name command label prop remove)
    (cond
     ((stringp item)
      (setq label
	    (if (string-match	; If an XEmacs separator
		 "^\\(-+\\|\
--:\\(\\(no\\|\\(sing\\|doub\\)le\\(Dashed\\)?\\)Line\\|\
shadow\\(Double\\)?Etched\\(In\\|Out\\)\\(Dash\\)?\\)\\)$"
		 item) ""		; use a single line separator.
	      item)))
     ((consp item)
      (setq label (setq name (car item)))
      (setq command (cdr item))
      (if (not (keymapp command))
	  (setq command (easy-menu-create-menu name command)))
      (if (null command)
	  ;; Invisible menu item. Don't insert into keymap.
	  (setq remove t)
	(when (and (symbolp command) (setq prop (get command 'menu-prop)))
	  (when (null (car prop))
	    (setq label (cadr prop))
	    (setq prop (cddr prop)))
	  (setq command (symbol-function command)))))
     ((vectorp item)
      (let* ((ilen (length item))
	     (active (if (> ilen 2) (or (aref item 2) ''nil) t))
	     (no-name (not (symbolp (setq command (aref item 1)))))
	     cache cache-specified)
	(setq label (setq name (aref item 0)))
	(if no-name (setq command (easy-menu-make-symbol command)))
	(if (and (symbolp active) (= ?: (aref (symbol-name active) 0)))
	    (let ((count 2)
		  keyword arg suffix visible style selected keys)
	      (setq active nil)
	      (while (> ilen count)
		(setq keyword (aref item count))
		(setq arg (aref item (1+ count)))
		(setq count (+ 2 count))
		(cond
		 ((eq keyword :visible) (setq visible (or arg ''nil)))
		 ((eq keyword :key-sequence)
		  (setq cache arg cache-specified t))
		 ((eq keyword :keys) (setq keys arg no-name nil))
		 ((eq keyword :label) (setq label arg))
		 ((eq keyword :active) (setq active (or arg ''nil)))
		 ((eq keyword :suffix) (setq suffix arg))
		 ((eq keyword :style) (setq style arg))
		 ((eq keyword :selected) (setq selected (or arg ''nil)))))
	      (if (stringp suffix)
		  (setq label (if (stringp label) (concat label " " suffix)
				(list 'concat label (concat " " suffix)))))
	      (if (and selected
		       (setq style (assq style easy-menu-button-prefix)))
		  (setq prop (cons :button
				   (cons (cons (cdr style) (or selected ''nil))
					 prop))))
	      (when (stringp keys)
		 (if (string-match "^[^\\]*\\(\\\\\\[\\([^]]+\\)]\\)[^\\]*$"
				   keys)
		     (let ((prefix
			    (if (< (match-beginning 0) (match-beginning 1))
				(substring keys 0 (match-beginning 1))))
			   (postfix
			    (if (< (match-end 1) (match-end 0))
				(substring keys (match-end 1))))
			   (cmd (intern (substring keys (match-beginning 2)
						   (match-end 2)))))
		       (setq keys (and (or prefix postfix)
				       (cons prefix postfix)))
		       (setq keys
			     (and (or keys (not (eq command cmd)))
				  (cons cmd keys))))
		   (setq cache-specified nil))
		 (if keys (setq prop (cons :keys (cons keys prop)))))
	      (if (and visible (not (easy-menu-always-true visible)))
		  (if (equal visible ''nil)
		      ;; Invisible menu item. Don't insert into keymap.
		      (setq remove t)
		    (setq prop (cons :visible (cons visible prop)))))))
	(if (and active (not (easy-menu-always-true active)))
	    (setq prop (cons :enable (cons active prop))))
	(if (and (or no-name cache-specified)
		 (or (null cache) (stringp cache) (vectorp cache)))
	    (setq prop (cons :key-sequence (cons cache prop))))))
     (t (error "Invalid menu item in easymenu.")))
    (easy-menu-define-key menu (if (stringp name) (intern name) name)
			  (and (not remove)
			       (cons 'menu-item
				     (cons label
					   (and name (cons command prop)))))
			  (if (stringp before) (intern before) before))))

(defun easy-menu-define-key (menu key item &optional before)
  ;; Add binding in MENU for KEY => ITEM.  Similar to `define-key-after'.
  ;; If KEY is not nil then delete any duplications. If ITEM is nil, then
  ;; don't insert, only delete.
  ;; Optional argument BEFORE is nil or a key in MENU.  If BEFORE is not nil
  ;; put binding before BEFORE in MENU, otherwise if binding is already
  ;; present in MENU, just change it, otherwise put it last in MENU.
  (let ((inserted (null item))		; Fake already inserted.
	tail done)
    (while (not done)
      (cond
       ((or (setq done (or (null (cdr menu)) (keymapp (cdr menu))))
	    (and before (equal (car-safe (cadr menu)) before)))
	;; If key is nil, stop here, otherwise keep going past the
	;; inserted element so we can delete any duplications that come
	;; later.
	(if (null key) (setq done t))
	(unless inserted		; Don't insert more than once.
	  (setcdr menu (cons (cons key item) (cdr menu)))
	  (setq inserted t)
	  (setq menu (cdr menu)))
	(setq menu (cdr menu)))
       ((and key (equal (car-safe (cadr menu)) key))
	(if (or inserted		; Already inserted or
		(and before		;  wanted elsewhere and
		     (setq tail (cddr menu)) ; not last item and not
		     (not (keymapp tail))
		     (not (equal (car-safe (car tail)) before)))) ; in position
	    (setcdr menu (cddr menu))	; Remove item.
	  (setcdr (cadr menu) item)	; Change item.
	  (setq inserted t)
	  (setq menu (cdr menu))))
       (t (setq menu (cdr menu)))))))
       
(defun easy-menu-always-true (x)
  ;; Return true if X never evaluates to nil.
  (if (consp x) (and (eq (car x) 'quote) (cadr x))
    (or (eq x t) (not (symbolp x)))))

(defvar easy-menu-item-count 0)

(defun easy-menu-make-symbol (callback)
  ;; Return a unique symbol with CALLBACK as function value.
  (let ((command
	 (make-symbol (format "menu-function-%d" easy-menu-item-count))))
    (setq easy-menu-item-count (1+ easy-menu-item-count))
    (fset command
	  (if (keymapp callback) callback
	    `(lambda () (interactive) ,callback)))
    command))

;;;###autoload
(defun easy-menu-change (path name items &optional before)
  "Change menu found at PATH as item NAME to contain ITEMS.
PATH is a list of strings for locating the menu containing NAME in the
menu bar.  ITEMS is a list of menu items, as in `easy-menu-define'.
These items entirely replace the previous items in that map.
If NAME is not present in the menu located by PATH, then add item NAME to
that menu. If the optional argument BEFORE is present add NAME in menu
just before BEFORE, otherwise add at end of menu.

Either call this from `menu-bar-update-hook' or use a menu filter,
to implement dynamic menus."
  (easy-menu-add-item nil path (cons name items) before))

;; XEmacs needs the following two functions to add and remove menus.
;; In Emacs this is done automatically when switching keymaps, so
;; here easy-menu-remove is a noop and easy-menu-add only precalculates
;; equivalent keybindings (if easy-menu-precalculate-equivalent-keybindings
;; is on).
(defun easy-menu-remove (menu))

(defun easy-menu-add (menu &optional map)
  "Maybe precalculate equivalent key bindings.
Do it if `easy-menu-precalculate-equivalent-keybindings' is on,"
  (when easy-menu-precalculate-equivalent-keybindings
    (if (and (symbolp menu) (not (keymapp menu)) (boundp menu))
	(setq menu (symbol-value menu)))
    (if (keymapp menu) (x-popup-menu nil menu))))

(defun easy-menu-add-item (menu path item &optional before)
  "At the end of the submenu of MENU with path PATH add ITEM.
If ITEM is already present in this submenu, then this item will be changed.
otherwise ITEM will be added at the end of the submenu, unless the optional
argument BEFORE is present, in which case ITEM will instead be added
before the item named BEFORE.
MENU is either a symbol, which have earlier been used as the first
argument in a call to `easy-menu-define', or the value of such a symbol
i.e. a menu, or nil which stands for the menu-bar itself.
PATH is a list of strings for locating the submenu where ITEM is to be
added.  If PATH is nil, MENU itself is used.  Otherwise, the first
element should be the name of a submenu directly under MENU.  This
submenu is then traversed recursively with the remaining elements of PATH.
ITEM is either defined as in `easy-menu-define' or a menu defined earlier
by `easy-menu-define' or `easy-menu-create-menu'."
  (setq menu (easy-menu-get-map menu path))
  (if (or (keymapp item)
	  (and (symbolp item) (keymapp (symbol-value item))))
      ;; Item is a keymap, find the prompt string and use as item name.
      (let ((tail (easy-menu-get-map item nil)) name)
	(if (not (keymapp item)) (setq item tail))
	(while (and (null name) (consp (setq tail (cdr tail)))
		    (not (keymapp tail)))
	  (if (stringp (car tail)) (setq name (car tail)) ; Got a name.
	    (setq tail (cdr tail))))
	(setq item (cons name item))))
  (easy-menu-do-add-item menu item before))

(defun easy-menu-item-present-p (menu path name)
  "In submenu of MENU with path PATH, return true iff item NAME is present.
MENU and PATH are defined as in `easy-menu-add-item'.
NAME should be a string, the name of the element to be looked for."
  (lookup-key (easy-menu-get-map menu path) (vector (intern name))))

(defun easy-menu-remove-item (menu path name)
  "From submenu of MENU with path PATH remove item NAME.
MENU and PATH are defined as in `easy-menu-add-item'.
NAME should be a string, the name of the element to be removed."
  (easy-menu-define-key (easy-menu-get-map menu path) (intern name) nil))

(defun easy-menu-get-map (menu path)
  ;; Return a sparse keymap in which to add or remove an item.
  ;; MENU and PATH are as defined in `easy-menu-add-item'.
  (if (null menu)
      (setq menu (key-binding (vconcat '(menu-bar) (mapcar 'intern path))))
    (if (and (symbolp menu) (not (keymapp menu)))
	(setq menu (symbol-value menu)))
    (if path (setq menu (lookup-key menu (vconcat (mapcar 'intern path))))))
  (while (and (symbolp menu) (keymapp menu))
    (setq menu (symbol-function menu)))
  (or (keymapp menu) (error "Malformed menu in easy-menu: (%s)" menu))
  menu)

(provide 'easymenu)

;;; easymenu.el ends here
