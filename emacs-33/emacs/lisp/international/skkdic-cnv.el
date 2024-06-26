;;; skkdic-cnv.el --- Convert a SKK dictionary for `skkdic-utl'

;; Copyright (C) 1995 Electrotechnical Laboratory, JAPAN.
;; Licensed to the Free Software Foundation.

;; Keywords: mule, multilingual, Japanese, SKK

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

;; SKK is a Japanese input method running on Mule created by Masahiko
;; Sato <masahiko@sato.riec.tohoku.ac.jp>.  Here we provide utilities
;; to handle a dictionary distributed with SKK so that a different
;; input method (e.g. quail-japanese) can utilize the dictionary.

;; The format of SKK dictionary is quite simple.  Each line has the
;; form "KANASTRING /CONV1/CONV2/.../" which means KANASTRING ($B2>L>J8(B
;; $B;zNs(B) can be converted to one of CONVi.  CONVi is a Kanji ($B4A;z(B)
;; and Kana ($B2>L>(B) mixed string.
;;
;; KANASTRING may have a trailing ASCII letter for Okurigana ($BAw$j2>L>(B)
;; information.  For instance, the trailing letter `k' means that one
;; of the following Okurigana is allowed: $B$+$-$/$1$3(B.  So, in that
;; case, the string "KANASTRING$B$/(B" can be converted to one of "CONV1$B$/(B",
;; CONV2$B$/(B, ...

;;; Code:

;; Name of a file to generate from SKK dictionary.
(defvar skkdic-filename "skkdic.el")

;; To make a generated skkdic.el smaller.
(make-coding-system
 'iso-2022-7bit-short
 2 ?J
 "Like `iso-2022-7bit' but no ASCII designation before SPC."
 '(ascii nil nil nil t t nil t))

(defun skkdic-convert-okuri-ari (skkbuf buf)
  (message "Processing OKURI-ARI entries ...")
  (goto-char (point-min))
  (save-excursion
    (set-buffer buf)
    (insert ";; Setting okuri-ari entries.\n"
	    "(skkdic-set-okuri-ari\n"))
  (while (not (eobp))
    (let ((from (point))
	  to)
      (end-of-line)
      (setq to (point))

      (save-excursion
	(set-buffer buf)
	(insert-buffer-substring skkbuf from to)
	(beginning-of-line)
	(insert "\"")
	(search-forward " ")
	(delete-char 1)			; delete the first '/'
	(let ((p (point)))
	  (end-of-line)
	  (delete-char -1)		; delete the last '/'
	  (subst-char-in-region p (point) ?/ ? 'noundo))
	(insert "\"\n"))

      (forward-line 1)))
  (save-excursion
    (set-buffer buf)
    (insert ")\n\n")))

(defconst skkdic-postfix-list '(skkdic-postfix-list))

(defconst skkdic-postfix-data
  '(("$B$$$-(B" "$B9T(B")
    ("$B$,$+$j(B" "$B78(B")
    ("$B$,$/(B" "$B3X(B")
    ("$B$,$o(B" "$B@n(B")
    ("$B$7$c(B" "$B<R(B")
    ("$B$7$e$&(B" "$B=8(B")
    ("$B$7$g$&(B" "$B>^(B" "$B>k(B")
    ("$B$8$g$&(B" "$B>k(B")
    ("$B$;$s(B" "$B@~(B")
    ("$B$@$1(B" "$B3Y(B")
    ("$B$A$c$/(B" "$BCe(B")
    ("$B$F$s(B" "$BE9(B")
    ("$B$H$&$2(B" "$BF=(B")
    ("$B$I$*$j(B" "$BDL$j(B")
    ("$B$d$^(B" "$B;3(B")
    ("$B$P$7(B" "$B66(B")
    ("$B$O$D(B" "$BH/(B")
    ("$B$b$/(B" "$BL\(B")
    ("$B$f$-(B" "$B9T(B")))

(defun skkdic-convert-postfix (skkbuf buf)
  (message "Processing POSTFIX entries ...")
  (goto-char (point-min))
  (save-excursion
    (set-buffer buf)
    (insert ";; Setting postfix entries.\n"
	    "(skkdic-set-postfix\n"))

  ;; Initialize SKKDIC-POSTFIX-LIST by predefined data
  ;; SKKDIC-POSTFIX-DATA.
  (save-excursion
    (set-buffer buf)
    (let ((l skkdic-postfix-data)
	  kana candidates entry)
      (while l
	(setq kana (car (car l)) candidates (cdr (car l)))
	(insert "\"" kana)
	(while candidates
	  (insert " " (car candidates))
	  (setq entry (lookup-nested-alist (car candidates)
					   skkdic-postfix-list nil nil t))
	  (if (consp (car entry))
	      (setcar entry (cons kana (car entry)))
	    (set-nested-alist (car candidates) (list kana)
			      skkdic-postfix-list))
	  (setq candidates (cdr candidates)))
	(insert "\"\n")
	(setq l (cdr l)))))

  ;; Search postfix entries.
  (while (re-search-forward "^[#<>?]\\(\\cH+\\) " nil t)
    (let ((kana (match-string 1))
	  str candidates)
      (while (looking-at "/[#0-9 ]*\\([^/\n]*\\)/")
	(setq str (match-string 1))
	(if (not (member str candidates))
	    (setq candidates (cons str candidates)))
	(goto-char (match-end 1)))
      (save-excursion
	(set-buffer buf)
	(insert "\"" kana)
	(while candidates
	  (insert " " (car candidates))
	  (let ((entry (lookup-nested-alist (car candidates)
					    skkdic-postfix-list nil nil t)))
	    (if (consp (car entry))
		(if (not (member kana (car entry)))
		    (setcar entry (cons kana (car entry))))
	      (set-nested-alist (car candidates) (list kana)
				skkdic-postfix-list)))
	  (setq candidates (cdr candidates)))
	(insert "\"\n"))))
  (save-excursion
    (set-buffer buf)
    (insert ")\n\n")))
	  
(defconst skkdic-prefix-list '(skkdic-prefix-list))

(defun skkdic-convert-prefix (skkbuf buf)
  (message "Processing PREFIX entries ...")
  (goto-char (point-min))
  (save-excursion
    (set-buffer buf)
    (insert ";; Setting prefix entries.\n"
	    "(skkdic-set-prefix\n"))
  (save-excursion
    (while (re-search-forward "^\\(\\cH+\\)[<>?] " nil t)
      (let ((kana (match-string 1))
	    str candidates)
	(while (looking-at "/\\([^/\n]+\\)/")
	  (setq str (match-string 1))
	  (if (not (member str candidates))
	      (setq candidates (cons str candidates)))
	  (goto-char (match-end 1)))
	(save-excursion
	  (set-buffer buf)
	  (insert "\"" kana)
	  (while candidates
	    (insert " " (car candidates))
	    (set-nested-alist (car candidates) kana skkdic-prefix-list)
	    (setq candidates (cdr candidates)))
	  (insert "\"\n")))))
  (save-excursion
    (set-buffer buf)
    (insert ")\n\n")))
	  
;; FROM and TO point the head and tail of "/J../J../.../".
(defun skkdic-get-candidate-list (from to)
  (let (candidates)
    (goto-char from)
    (while (re-search-forward "/\\cj+" to t)
      (setq candidates (cons (buffer-substring (1+ (match-beginning 0))
					       (match-end 0))
			     candidates)))
    candidates))

;; Return entry for STR from nested alist ALIST.
(defsubst skkdic-get-entry (str alist)
  (car (lookup-nested-alist str alist nil nil t)))


(defconst skkdic-word-list '(skkdic-word-list))

;; Return t if substring of STR (between FROM and TO) can be broken up
;; to chunks all of which can be derived from another entry in SKK
;; dictionary.  SKKBUF is the buffer where the original SKK dictionary
;; is visited, KANA is the current entry for STR.  FIRST is t iff this
;; is called at top level.

(defun skkdic-breakup-string (skkbuf kana str from to &optional first)
  (let ((len (- to from)))
    (or (and (>= len 2)
	     (let ((min-idx (+ from 2))
		   (idx (if first (1- to ) to))
		   (found nil))
	       (while (and (not found) (>= idx min-idx))
		 (let ((kana2-list (skkdic-get-entry
				    (substring str from idx)
				    skkdic-word-list)))
		   (if (or (and (consp kana2-list)
				(let ((kana-len (length kana))
				      kana2)
				  (catch 'skkdic-tag
				    (while kana2-list
				      (setq kana2 (car kana2-list))
				      (if (string-match kana2 kana)
					  (throw 'skkdic-tag t))
				      (setq kana2-list (cdr kana2-list)))))
				(or (= idx to)
				    (skkdic-breakup-string skkbuf kana str
							   idx to)))
			   (and (stringp kana2-list)
				(string-match kana2-list kana)))
		       (setq found t)
		     (setq idx (1- idx)))))
	       found))
	(and first
	     (> len 2)
	     (let ((kana2 (skkdic-get-entry
			   (substring str from (1+ from))
			   skkdic-prefix-list)))
	       (and (stringp kana2)
		    (eq (string-match kana2 kana) 0)))
	     (skkdic-breakup-string skkbuf kana str (1+ from) to))
	(and (not first)
	     (>= len 1)
	     (let ((kana2-list (skkdic-get-entry
				(substring str from to)
				skkdic-postfix-list)))
	       (and (consp kana2-list)
		    (let (kana2)
		      (catch 'skkdic-tag
			(while kana2-list
			  (setq kana2 (car kana2-list))
			  (if (string= kana2
				       (substring kana (- (length kana2))))
			      (throw 'skkdic-tag t))
			  (setq kana2-list (cdr kana2-list)))))))))))

;; Return list of candidates which excludes some from CANDIDATES.
;; Excluded candidates can be derived from another entry.

(defun skkdic-reduced-candidates (skkbuf kana candidates)
  (let (elt l)
    (while candidates
      (setq elt (car candidates))
      (if (or (= (length elt) 1)
	      (and (string-match "^\\cj" elt)
		   (not (skkdic-breakup-string skkbuf kana elt 0 (length elt)
					       'first))))
	  (setq l (cons elt l)))
      (setq candidates (cdr candidates)))
    (nreverse l)))

(defconst skkdic-okuri-nasi-entries (list nil))
(defconst skkdic-okuri-nasi-entries-count 0)

(defun skkdic-collect-okuri-nasi ()
  (message "Collecting OKURI-NASI entries ...")
  (save-excursion
    (let ((prev-ratio 0)
	  ratio)
      (while (re-search-forward "^\\(\\cH+\\) \\(/\\cj.*\\)/$" nil t)
	(let ((kana (match-string 1))
	      (candidates (skkdic-get-candidate-list (match-beginning 2)
						     (match-end 2))))
	  (setq skkdic-okuri-nasi-entries
		(cons (cons kana candidates) skkdic-okuri-nasi-entries)
		skkdic-okuri-nasi-entries-count
		(1+ skkdic-okuri-nasi-entries-count))
	  (setq ratio (floor (/ (* (point) 100.0) (point-max))))
	  (if (/= ratio prev-ratio)
	      (progn
		(message "collected %2d%% %s ..." ratio kana)
		(setq prev-ratio ratio)))
	  (while candidates
	    (let ((entry (lookup-nested-alist (car candidates)
					      skkdic-word-list nil nil t)))
	      (if (consp (car entry))
		  (setcar entry (cons kana (car entry)))
		(set-nested-alist (car candidates) (list kana)
				  skkdic-word-list)))
	    (setq candidates (cdr candidates))))))))

(defun skkdic-convert-okuri-nasi (skkbuf buf)
  (message "Processing OKURI-NASI entries ...")
  (save-excursion
    (set-buffer buf)
    (insert ";; Setting okuri-nasi entries.\n"
	    "(skkdic-set-okuri-nasi\n")
    (let ((l (nreverse skkdic-okuri-nasi-entries))
	  (count 0)
	  (prev-ratio 0)
	  ratio)
      (while l
	(let ((kana (car (car l)))
	      (candidates (cdr (car l))))
	  (setq ratio (/ (* count 1000) skkdic-okuri-nasi-entries-count)
		count (1+ count))
	  (if (/= prev-ratio (/ ratio 10))
	      (progn
		(message "processed %2d%% %s ..." (/ ratio 10) kana)
		(setq prev-ratio (/ ratio 10))))
	  (if (setq candidates
		    (skkdic-reduced-candidates skkbuf kana candidates))
	      (progn
		(insert "\"" kana)
		(while candidates
		  (insert " " (car candidates))
		  (setq candidates (cdr candidates)))
		(insert "\"\n"))))
	(setq l (cdr l))))
    (insert ")\n\n")))

(defun skkdic-convert (filename &optional dirname)
  "Convert SKK dictionary of FILENAME into the file \"skkdic.el\".
Optional argument DIRNAME if specified is the directory name under which
the generated \"skkdic.el\" is saved."
  (interactive "FSKK dictionary file: ")
  (message "Reading file \"%s\" ..." filename)
  (let ((skkbuf(find-file-noselect (expand-file-name filename)))
	(buf (get-buffer-create "*skkdic-work*")))
    (save-excursion
      ;; Setup and generate the header part of working buffer.
      (set-buffer buf)
      (erase-buffer)
      (buffer-disable-undo)
      (insert ";; skkdic.el -- dictionary for Japanese input method\n"
	      ";;\tGenerated by the command `skkdic-convert'\n"
	      ";;\tDate: " (current-time-string) "\n"
	      ";;\tOriginal SKK dictionary file: "
	      (file-name-nondirectory filename)
	      "\n\n"
	      ";;; Comment:\n\n"
	      ";; Do byte-compile this file again after any modification.\n\n"
	      ";;; Start of the header of the original SKK dictionary.\n\n")
      (set-buffer skkbuf)
      (widen)
      (goto-char 1)
      (let (pos)
	(search-forward ";; okuri-ari")
	(forward-line 1)
	(setq pos (point))
	(set-buffer buf)
	(insert-buffer-substring skkbuf 1 pos))
      (insert "\n"
	      ";;; Code:\n\n")

      ;; Generate the body part of working buffer.
      (set-buffer skkbuf)
      (let ((from (point))
	    to)
	;; Convert okuri-ari entries.
	(search-forward ";; okuri-nasi")
	(beginning-of-line)
	(setq to (point))
	(narrow-to-region from to)
	(skkdic-convert-okuri-ari skkbuf buf)
	(widen)

	;; Convert okuri-nasi postfix entries.
	(goto-char to)
	(forward-line 1)
	(setq from (point))
	(re-search-forward "^\\cH")
	(setq to (match-beginning 0))
	(narrow-to-region from to)
	(skkdic-convert-postfix skkbuf buf)
	(widen)

	;; Convert okuri-nasi prefix entries.
	(goto-char to)
	(skkdic-convert-prefix skkbuf buf)

	;; 
	(skkdic-collect-okuri-nasi)

	;; Convert okuri-nasi general entries.
	(skkdic-convert-okuri-nasi skkbuf buf)

	;; Postfix
	(save-excursion
	  (set-buffer buf)
	  (goto-char (point-max))
	  (insert ";;\n(provide 'skkdic)\n\n;; skkdic.el ends here\n")))

      ;; Save the working buffer.
      (set-buffer buf)
      (set-visited-file-name (expand-file-name skkdic-filename dirname) t)
      (set-buffer-file-coding-system 'iso-2022-7bit-short)
      (save-buffer 0))
    (kill-buffer skkbuf)
    (switch-to-buffer buf)))

(defun batch-skkdic-convert ()
  "Run `skkdic-convert' on the files remaining on the command line.
Use this from the command line, with `-batch';
it won't work in an interactive Emacs.
For example, invoke:
  % emacs -batch -l skkconv -f batch-skkdic-convert SKK-JISYO.L
to generate  \"skkdic.el\" from SKK dictionary file \"SKK-JISYO.L\".
To get complete usage, invoke:
 % emacs -batch -l skkconv -f batch-skkdic-convert -h"
  (defvar command-line-args-left)	; Avoid compiler warning.
  (if (not noninteractive)
      (error "`batch-skkdic-convert' should be used only with -batch"))
  (if (string= (car command-line-args-left) "-h")
      (progn
	(message "To convert SKK-JISYO.L into skkdic.el:")
	(message "  %% emacs -batch -l skkdic-conv -f batch-skkdic-convert SKK-JISYO.L")
	(message "To convert SKK-JISYO.L into DIR/skkdic.el:")
	(message "  %% emacs -batch -l skkdic-conv -f batch-skkdic-convert -dir DIR SKK-JISYO.L"))
    (let (targetdir filename)
      (if (string= (car command-line-args-left) "-dir")
	  (progn
	    (setq command-line-args-left (cdr command-line-args-left))
	    (setq targetdir (expand-file-name (car command-line-args-left)))
	    (setq command-line-args-left (cdr command-line-args-left))))
      (setq filename (expand-file-name (car command-line-args-left)))
      (message "Converting %s to skkdic.el ..." filename)
      (message "It takes around 10 minutes even on Sun SS20.")
      (skkdic-convert filename targetdir)
      (message "Do byte-compile the created file by:")
      (message "  %% emacs -batch -l skkdic-cnv -f batch-byte-compile skkdic.el")
      (message  "                 ^^^^^^^^^^^^^ -- Don't forget this option!")
      ))
  (kill-emacs 0))


;; The following macros are expanded at byte-compiling time so that
;; compiled code can be loaded quickly.

(defun skkdic-get-kana-compact-codes (kana)
  (let* ((len (length kana))
	 (vec (make-vector len 0))
	 (i 0)
	 ch)
    (while (< i len)
      (setq ch (aref kana i))
      (aset vec i
	    (if (< ch 128)		; CH is an ASCII letter for OKURIGANA,
		(- ch)			;  represented by a negative code.
	      (if (= ch ?$B!<(B)		; `$B!<(B' is represented by 0.
		  0
		(- (nth 2 (split-char ch)) 32))))
      (setq i (1+ i)))
    vec))

(defun skkdic-extract-conversion-data (entry)
  (string-match "^\\cj+[a-z]* " entry)
  (let ((kana (substring entry (match-beginning 0) (1- (match-end 0))))
	(i (match-end 0))
	candidates)
    (while (string-match "[^ ]+" entry i)
      (setq candidates (cons (match-string 0 entry) candidates))
      (setq i (match-end 0)))
    (cons (skkdic-get-kana-compact-codes kana) candidates)))

(defmacro skkdic-set-okuri-ari (&rest entries)
  `(defconst skkdic-okuri-ari
     ',(let ((l entries)
	     (map '(skkdic-okuri-ari))
	     entry)
	 (while l
	   (setq entry (skkdic-extract-conversion-data (car l)))
	   (set-nested-alist (car entry) (cdr entry) map)
	   (setq l (cdr l)))
	 map)))

(defmacro skkdic-set-postfix (&rest entries)
  `(defconst skkdic-postfix
     ',(let ((l entries)
	     (map '(nil))
	     (longest 1)
	     len entry)
	 (while l
	   (setq entry (skkdic-extract-conversion-data (car l)))
	   (setq len (length (car entry)))
	   (if (> len longest)
	       (setq longest len))
	   (let ((entry2 (lookup-nested-alist (car entry) map nil nil t)))
	     (if (consp (car entry2))
		 (let ((conversions (cdr entry)))
		   (while conversions
		     (if (not (member (car conversions) (car entry2)))
			 (setcar entry2 (cons (car conversions) (car entry2))))
		     (setq conversions (cdr conversions))))
	       (set-nested-alist (car entry) (cdr entry) map)))
	   (setq l (cdr l)))
	 (setcar map longest)
	 map)))

(defmacro skkdic-set-prefix (&rest entries)
  `(defconst skkdic-prefix
     ',(let ((l entries)
	     (map '(nil))
	     (longest 1)
	     len entry)
	 (while l
	   (setq entry (skkdic-extract-conversion-data (car l)))
	   (setq len (length (car entry)))
	   (if (> len longest)
	       (setq longest len))
	   (let ((entry2 (lookup-nested-alist (car entry) map len nil t)))
	     (if (consp (car entry2))
		 (let ((conversions (cdr entry)))
		   (while conversions
		     (if (not (member (car conversions) (car entry2)))
			 (setcar entry2 (cons (car conversions) (car entry2))))
		     (setq conversions (cdr conversions))))
	       (set-nested-alist (car entry) (cdr entry) map len)))
	   (setq l (cdr l)))
	 (setcar map longest)
	 map)))

(defmacro skkdic-set-okuri-nasi (&rest entries)
  `(defconst skkdic-okuri-nasi
     ',(let ((l entries)
	     (map '(skdic-okuri-nasi))
	     (count 0)
	     entry)
	 (while l
	   (setq count (1+ count))
	   (if (= (% count 10) 0)
	       (message (format "%d entries" count)))
	   (setq entry (skkdic-extract-conversion-data (car l)))
	   (set-nested-alist (car entry) (cdr entry) map)
	   (setq l (cdr l)))
	 map)))

;; skkdic-cnv.el ends here
