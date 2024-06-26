;;; ccl.el --- CCL (Code Conversion Language) compiler

;; Copyright (C) 1995 Electrotechnical Laboratory, JAPAN.
;; Licensed to the Free Software Foundation.

;; Keywords: CCL, mule, multilingual, character set, coding-system

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

;; CCL (Code Conversion Language) is a simple programming language to
;; be used for various kind of code conversion.  CCL program is
;; compiled to CCL code (vector of integers) and executed by CCL
;; interpreter of Emacs.
;;
;; CCL is used for code conversion at process I/O and file I/O for
;; non-standard coding-system.  In addition, it is used for
;; calculating a code point of X's font from a character code.
;; However, since CCL is designed as a powerful programming language,
;; it can be used for more generic calculation.  For instance,
;; combination of three or more arithmetic operations can be
;; calculated faster than Emacs Lisp.
;;
;; Here's the syntax of CCL program in BNF notation.
;;
;; CCL_PROGRAM :=
;;	(BUFFER_MAGNIFICATION
;;	 CCL_MAIN_BLOCK
;;	 [ CCL_EOF_BLOCK ])
;;
;; BUFFER_MAGNIFICATION := integer
;; CCL_MAIN_BLOCK := CCL_BLOCK
;; CCL_EOF_BLOCK := CCL_BLOCK
;;
;; CCL_BLOCK :=
;;	STATEMENT | (STATEMENT [STATEMENT ...])
;; STATEMENT :=
;;	SET | IF | BRANCH | LOOP | REPEAT | BREAK | READ | WRITE | CALL
;;
;; SET :=
;;	(REG = EXPRESSION)
;;	| (REG ASSIGNMENT_OPERATOR EXPRESSION)
;;	| integer
;;
;; EXPRESSION := ARG | (EXPRESSION OPERATOR ARG)
;;
;; IF := (if EXPRESSION CCL_BLOCK CCL_BLOCK)
;; BRANCH := (branch EXPRESSION CCL_BLOCK [CCL_BLOCK ...])
;; LOOP := (loop STATEMENT [STATEMENT ...])
;; BREAK := (break)
;; REPEAT :=
;;	(repeat)
;;	| (write-repeat [REG | integer | string])
;;	| (write-read-repeat REG [integer | ARRAY])
;; READ :=
;;	(read REG ...)
;;	| (read-if (REG OPERATOR ARG) CCL_BLOCK CCL_BLOCK)
;;	| (read-branch REG CCL_BLOCK [CCL_BLOCK ...])
;;      | (read-multibyte-character REG {charset} REG {code-point})
;; WRITE :=
;;	(write REG ...)
;;	| (write EXPRESSION)
;;	| (write integer) | (write string) | (write REG ARRAY)
;;	| string
;;      | (write-multibyte-character REG(charset) REG(codepoint))
;; TRANSLATE :=
;;      (translate-character REG(table) REG(charset) REG(codepoint))
;;      | (translate-character SYMBOL REG(charset) REG(codepoint))
;; MAP :=
;;      (iterate-multiple-map REG REG MAP-IDs)
;;      | (map-multiple REG REG (MAP-SET))
;;      | (map-single REG REG MAP-ID)
;; MAP-IDs := MAP-ID ...
;; MAP-SET := MAP-IDs | (MAP-IDs) MAP-SET
;; MAP-ID := integer
;;
;; CALL := (call ccl-program-name)
;; END := (end)
;;
;; REG := r0 | r1 | r2 | r3 | r4 | r5 | r6 | r7
;; ARG := REG | integer
;; OPERATOR :=
;;	+ | - | * | / | % | & | '|' | ^ | << | >> | <8 | >8 | //
;;	| < | > | == | <= | >= | != | de-sjis | en-sjis
;; ASSIGNMENT_OPERATOR :=
;;	+= | -= | *= | /= | %= | &= | '|=' | ^= | <<= | >>=
;; ARRAY := '[' interger ... ']'

;;; Code:

(defgroup ccl nil
  "CCL (Code Conversion Language) compiler."
  :prefix "ccl-"
  :group 'i18n)

(defconst ccl-command-table
  [if branch loop break repeat write-repeat write-read-repeat
      read read-if read-branch write call end
      read-multibyte-character write-multibyte-character
      translate-character
      iterate-multiple-map map-multiple map-single]
  "Vector of CCL commands (symbols).")

;; Put a property to each symbol of CCL commands for the compiler.
(let (op (i 0) (len (length ccl-command-table)))
  (while (< i len)
    (setq op (aref ccl-command-table i))
    (put op 'ccl-compile-function (intern (format "ccl-compile-%s" op)))
    (setq i (1+ i))))

(defconst ccl-code-table
  [set-register
   set-short-const
   set-const
   set-array
   jump
   jump-cond
   write-register-jump
   write-register-read-jump
   write-const-jump
   write-const-read-jump
   write-string-jump
   write-array-read-jump
   read-jump
   branch
   read-register
   write-expr-const
   read-branch
   write-register
   write-expr-register
   call
   write-const-string
   write-array
   end
   set-assign-expr-const
   set-assign-expr-register
   set-expr-const
   set-expr-register
   jump-cond-expr-const
   jump-cond-expr-register
   read-jump-cond-expr-const
   read-jump-cond-expr-register
   ex-cmd
   ]
  "Vector of CCL compiled codes (symbols).")

(defconst ccl-extended-code-table
  [read-multibyte-character
   write-multibyte-character
   translate-character
   translate-character-const-tbl
   nil nil nil nil nil nil nil nil nil nil nil nil ; 0x04-0x0f
   iterate-multiple-map
   map-multiple
   map-single
   ]
  "Vector of CCL extended compiled codes (symbols).")

;; Put a property to each symbol of CCL codes for the disassembler.
(let (code (i 0) (len (length ccl-code-table)))
  (while (< i len)
    (setq code (aref ccl-code-table i))
    (put code 'ccl-code i)
    (put code 'ccl-dump-function (intern (format "ccl-dump-%s" code)))
    (setq i (1+ i))))

(let (code (i 0) (len (length ccl-extended-code-table)))
  (while (< i len)
    (setq code (aref ccl-extended-code-table i))
    (if code
	(progn
	  (put code 'ccl-ex-code i)
	  (put code 'ccl-dump-function (intern (format "ccl-dump-%s" code)))))
    (setq i (1+ i))))

(defconst ccl-jump-code-list
  '(jump jump-cond write-register-jump write-register-read-jump
    write-const-jump write-const-read-jump write-string-jump
    write-array-read-jump read-jump))

;; Put a property `jump-flag' to each CCL code which execute jump in
;; some way.
(let ((l ccl-jump-code-list))
  (while l
    (put (car l) 'jump-flag t)
    (setq l (cdr l))))

(defconst ccl-register-table
  [r0 r1 r2 r3 r4 r5 r6 r7]
  "Vector of CCL registers (symbols).")

;; Put a property to indicate register number to each symbol of CCL.
;; registers.
(let (reg (i 0) (len (length ccl-register-table)))
  (while (< i len)
    (setq reg (aref ccl-register-table i))
    (put reg 'ccl-register-number i)
    (setq i (1+ i))))

(defconst ccl-arith-table
  [+ - * / % & | ^ << >> <8 >8 // nil nil nil
   < > == <= >= != de-sjis en-sjis]
  "Vector of CCL arithmetic/logical operators (symbols).")

;; Put a property to each symbol of CCL operators for the compiler.
(let (arith (i 0) (len (length ccl-arith-table)))
  (while (< i len)
    (setq arith (aref ccl-arith-table i))
    (if arith (put arith 'ccl-arith-code i))
    (setq i (1+ i))))

(defconst ccl-assign-arith-table
  [+= -= *= /= %= &= |= ^= <<= >>= <8= >8= //=]
  "Vector of CCL assignment operators (symbols).")

;; Put a property to each symbol of CCL assignment operators for the compiler.
(let (arith (i 0) (len (length ccl-assign-arith-table)))
  (while (< i len)
    (setq arith (aref ccl-assign-arith-table i))
    (put arith 'ccl-self-arith-code i)
    (setq i (1+ i))))

(defvar ccl-program-vector nil
  "Working vector of CCL codes produced by CCL compiler.")
(defvar ccl-current-ic 0
  "The current index for `ccl-program-vector'.")

;; Embed integer DATA in `ccl-program-vector' at `ccl-current-ic' and
;; increment it.  If IC is specified, embed DATA at IC.
(defun ccl-embed-data (data &optional ic)
  (if ic
      (aset ccl-program-vector ic data)
    (aset ccl-program-vector ccl-current-ic data)
    (setq ccl-current-ic (1+ ccl-current-ic))))

;; Embed string STR of length LEN in `ccl-program-vector' at
;; `ccl-current-ic'.
(defun ccl-embed-string (len str)
  (let ((i 0))
    (while (< i len)
      (ccl-embed-data (logior (ash (aref str i) 16)
			       (if (< (1+ i) len)
				   (ash (aref str (1+ i)) 8)
				 0)
			       (if (< (+ i 2) len)
				   (aref str (+ i 2))
				 0)))
      (setq i (+ i 3)))))

;; Embed a relative jump address to `ccl-current-ic' in
;; `ccl-program-vector' at IC without altering the other bit field.
(defun ccl-embed-current-address (ic)
  (let ((relative (- ccl-current-ic (1+ ic))))
    (aset ccl-program-vector ic
	  (logior (aref ccl-program-vector ic) (ash relative 8)))))

;; Embed CCL code for the operation OP and arguments REG and DATA in
;; `ccl-program-vector' at `ccl-current-ic' in the following format.
;;	|----------------- integer (28-bit) ------------------|
;;	|------------ 20-bit ------------|- 3-bit --|- 5-bit -|
;;	|------------- DATA -------------|-- REG ---|-- OP ---|
;; If REG2 is specified, embed a code in the following format.
;;	|------- 17-bit ------|- 3-bit --|- 3-bit --|- 5-bit -|
;;	|-------- DATA -------|-- REG2 --|-- REG ---|-- OP ---|

;; If REG is a CCL register symbol (e.g. r0, r1...), the register
;; number is embedded.  If OP is one of unconditional jumps, DATA is
;; changed to an relative jump address.

(defun ccl-embed-code (op reg data &optional reg2)
  (if (and (> data 0) (get op 'jump-flag))
      ;; DATA is an absolute jump address.  Make it relative to the
      ;; next of jump code.
      (setq data (- data (1+ ccl-current-ic))))
  (let ((code (logior (get op 'ccl-code)
		      (ash
		       (if (symbolp reg) (get reg 'ccl-register-number) reg) 5)
		      (if reg2
			  (logior (ash (get reg2 'ccl-register-number) 8)
				  (ash data 11))
			(ash data 8)))))
    (aset ccl-program-vector ccl-current-ic code)
    (setq ccl-current-ic (1+ ccl-current-ic))))

;; extended ccl command format
;;	|- 14-bit -|- 3-bit --|- 3-bit --|- 3-bit --|- 5-bit -|
;;	|- EX-OP --|-- REG3 --|-- REG2 --|-- REG ---|-- OP ---|
(defun ccl-embed-extended-command (ex-op reg reg2 reg3)
  (let ((data (logior (ash (get ex-op 'ccl-ex-code) 3)
		      (if (symbolp reg3)
			  (get reg3 'ccl-register-number)
			0))))
    (ccl-embed-code 'ex-cmd reg data reg2)))

;; Just advance `ccl-current-ic' by INC.
(defun ccl-increment-ic (inc)
  (setq ccl-current-ic (+ ccl-current-ic inc)))

;;;###autoload
(defun ccl-program-p (obj)
  "T if OBJECT is a valid CCL compiled code."
  (and (vectorp obj)
       (let ((i 0) (len (length obj)) (flag t))
	 (if (> len 1)
	     (progn
	       (while (and flag (< i len))
		 (setq flag (integerp (aref obj i)))
		 (setq i (1+ i)))
	       flag)))))

;; If non-nil, index of the start of the current loop.
(defvar ccl-loop-head nil)
;; If non-nil, list of absolute addresses of the breaking points of
;; the current loop.
(defvar ccl-breaks nil)

;;;###autoload
(defun ccl-compile (ccl-program)
  "Return a comiled code of CCL-PROGRAM as a vector of integer."
  (if (or (null (consp ccl-program))
	  (null (integerp (car ccl-program)))
	  (null (listp (car (cdr ccl-program)))))
      (error "CCL: Invalid CCL program: %s" ccl-program))
  (if (null (vectorp ccl-program-vector))
      (setq ccl-program-vector (make-vector 8192 0)))
  (setq ccl-loop-head nil ccl-breaks nil)
  (setq ccl-current-ic 0)

  ;; The first element is the buffer magnification.
  (ccl-embed-data (car ccl-program))

  ;; The second element is the address of the start CCL code for
  ;; processing end of input buffer (we call it eof-processor).  We
  ;; set it later.
  (ccl-increment-ic 1)

  ;; Compile the main body of the CCL program.
  (ccl-compile-1 (car (cdr ccl-program)))

  ;; Embed the address of eof-processor.
  (ccl-embed-data ccl-current-ic 1)

  ;; Then compile eof-processor.
  (if (nth 2 ccl-program)
      (ccl-compile-1 (nth 2 ccl-program)))

  ;; At last, embed termination code.
  (ccl-embed-code 'end 0 0)

  (let ((vec (make-vector ccl-current-ic 0))
	(i 0))
    (while (< i ccl-current-ic)
      (aset vec i (aref ccl-program-vector i))
      (setq i (1+ i)))
    vec))

;; Signal syntax error.
(defun ccl-syntax-error (cmd)
  (error "CCL: Syntax error: %s" cmd))

;; Check if ARG is a valid CCL register.
(defun ccl-check-register (arg cmd)
  (if (get arg 'ccl-register-number)
      arg
    (error "CCL: Invalid register %s in %s." arg cmd)))

;; Check if ARG is a valid CCL command.
(defun ccl-check-compile-function (arg cmd)
  (or (get arg 'ccl-compile-function)
      (error "CCL: Invalid command: %s" cmd)))

;; In the following code, most ccl-compile-XXXX functions return t if
;; they end with unconditional jump, else return nil.

;; Compile CCL-BLOCK (see the syntax above).
(defun ccl-compile-1 (ccl-block)
  (let (unconditional-jump
	cmd)
    (if (or (integerp ccl-block)
	    (stringp ccl-block)
	    (and ccl-block (symbolp (car ccl-block))))
	;; This block consists of single statement.
	(setq ccl-block (list ccl-block)))

    ;; Now CCL-BLOCK is a list of statements.  Compile them one by
    ;; one.
    (while ccl-block
      (setq cmd (car ccl-block))
      (setq unconditional-jump
	    (cond ((integerp cmd)
		   ;; SET statement for the register 0.
		   (ccl-compile-set (list 'r0 '= cmd)))

		  ((stringp cmd)
		   ;; WRITE statement of string argument.
		   (ccl-compile-write-string cmd))

		  ((listp cmd)
		   ;; The other statements.
		   (cond ((eq (nth 1 cmd) '=)
			  ;; SET statement of the form `(REG = EXPRESSION)'.
			  (ccl-compile-set cmd))

			 ((and (symbolp (nth 1 cmd))
			       (get (nth 1 cmd) 'ccl-self-arith-code))
			  ;; SET statement with an assignment operation.
			  (ccl-compile-self-set cmd))

			 (t
			  (funcall (ccl-check-compile-function (car cmd) cmd)
				   cmd))))

		  (t
		   (ccl-syntax-error cmd))))
      (setq ccl-block (cdr ccl-block)))
    unconditional-jump))

(defconst ccl-max-short-const (ash 1 19))
(defconst ccl-min-short-const (ash -1 19))

;; Compile SET statement.
(defun ccl-compile-set (cmd)
  (let ((rrr (ccl-check-register (car cmd) cmd))
	(right (nth 2 cmd)))
    (cond ((listp right)
	   ;; CMD has the form `(RRR = (XXX OP YYY))'.
	   (ccl-compile-expression rrr right))

	  ((integerp right)
	   ;; CMD has the form `(RRR = integer)'.
	   (if (and (<= right ccl-max-short-const)
		    (>= right ccl-min-short-const))
	       (ccl-embed-code 'set-short-const rrr right)
	     (ccl-embed-code 'set-const rrr 0)
	     (ccl-embed-data right)))

	  (t
	   ;; CMD has the form `(RRR = rrr [ array ])'.
	   (ccl-check-register right cmd)
	   (let ((ary (nth 3 cmd)))
	     (if (vectorp ary)
		 (let ((i 0) (len (length ary)))
		   (ccl-embed-code 'set-array rrr len right)
		   (while (< i len)
		     (ccl-embed-data (aref ary i))
		     (setq i (1+ i))))
	       (ccl-embed-code 'set-register rrr 0 right))))))
  nil)

;; Compile SET statement with ASSIGNMENT_OPERATOR.
(defun ccl-compile-self-set (cmd)
  (let ((rrr (ccl-check-register (car cmd) cmd))
	(right (nth 2 cmd)))
    (if (listp right)
	;; CMD has the form `(RRR ASSIGN_OP (XXX OP YYY))', compile
	;; the right hand part as `(r7 = (XXX OP YYY))' (note: the
	;; register 7 can be used for storing temporary value).
	(progn
	  (ccl-compile-expression 'r7 right)
	  (setq right 'r7)))
    ;; Now CMD has the form `(RRR ASSIGN_OP ARG)'.  Compile it as
    ;; `(RRR = (RRR OP ARG))'.
    (ccl-compile-expression
     rrr
     (list rrr (intern (substring (symbol-name (nth 1 cmd)) 0 -1)) right)))
  nil)

;; Compile SET statement of the form `(RRR = EXPR)'.
(defun ccl-compile-expression (rrr expr)
  (let ((left (car expr))
	(op (get (nth 1 expr) 'ccl-arith-code))
	(right (nth 2 expr)))
    (if (listp left)
	(progn
	  ;; EXPR has the form `((EXPR2 OP2 ARG) OP RIGHT)'.  Compile
	  ;; the first term as `(r7 = (EXPR2 OP2 ARG)).'
	  (ccl-compile-expression 'r7 left)
	  (setq left 'r7)))

    ;; Now EXPR has the form (LEFT OP RIGHT).
    (if (eq rrr left)
	;; Compile this SET statement as `(RRR OP= RIGHT)'.
	(if (integerp right)
	    (progn
	      (ccl-embed-code 'set-assign-expr-const rrr (ash op 3) 'r0)
	      (ccl-embed-data right))
	  (ccl-check-register right expr)
	  (ccl-embed-code 'set-assign-expr-register rrr (ash op 3) right))

      ;; Compile this SET statement as `(RRR = (LEFT OP RIGHT))'.
      (if (integerp right)
	  (progn
	    (ccl-embed-code 'set-expr-const rrr (ash op 3) left)
	    (ccl-embed-data right))
	(ccl-check-register right expr)
	(ccl-embed-code 'set-expr-register
			rrr
			(logior (ash op 3) (get right 'ccl-register-number))
			left)))))

;; Compile WRITE statement with string argument.
(defun ccl-compile-write-string (str)
  (let ((len (length str)))
    (ccl-embed-code 'write-const-string 1 len)
    (ccl-embed-string len str))
  nil)

;; Compile IF statement of the form `(if CONDITION TRUE-PART FALSE-PART)'.
;; If READ-FLAG is non-nil, this statement has the form
;; `(read-if (REG OPERATOR ARG) TRUE-PART FALSE-PART)'.
(defun ccl-compile-if (cmd &optional read-flag)
  (if (and (/= (length cmd) 3) (/= (length cmd) 4))
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((condition (nth 1 cmd))
	(true-cmds (nth 2 cmd))
	(false-cmds (nth 3 cmd))
	jump-cond-address
	false-ic)
    (if (and (listp condition)
	     (listp (car condition)))
	;; If CONDITION is a nested expression, the inner expression
	;; should be compiled at first as SET statement, i.e.:
	;; `(if ((X OP2 Y) OP Z) ...)' is compiled into two statements:
	;; `(r7 = (X OP2 Y)) (if (r7 OP Z) ...)'.
	(progn
	  (ccl-compile-expression 'r7 (car condition))
	  (setq condition (cons 'r7 (cdr condition)))
	  (setq cmd (cons (car cmd)
			  (cons condition (cdr (cdr cmd)))))))

    (setq jump-cond-address ccl-current-ic)
    ;; Compile CONDITION.
    (if (symbolp condition)
	;; CONDITION is a register.
	(progn
	  (ccl-check-register condition cmd)
	  (ccl-embed-code 'jump-cond condition 0))
      ;; CONDITION is a simple expression of the form (RRR OP ARG).
      (let ((rrr (car condition))
	    (op (get (nth 1 condition) 'ccl-arith-code))
	    (arg (nth 2 condition)))
	(ccl-check-register rrr cmd)
	(if (integerp arg)
	    (progn
	      (ccl-embed-code (if read-flag 'read-jump-cond-expr-const
				'jump-cond-expr-const)
			      rrr 0)
	      (ccl-embed-data op)
	      (ccl-embed-data arg))
	  (ccl-check-register arg cmd)
	  (ccl-embed-code (if read-flag 'read-jump-cond-expr-register 
			    'jump-cond-expr-register)
			  rrr 0)
	  (ccl-embed-data op)
	  (ccl-embed-data (get arg 'ccl-register-number)))))

    ;; Compile TRUE-PART.
    (let ((unconditional-jump (ccl-compile-1 true-cmds)))
      (if (null false-cmds)
	  ;; This is the place to jump to if condition is false.
	  (ccl-embed-current-address jump-cond-address)
	(let (end-true-part-address)
	  (if (not unconditional-jump)
	      (progn
		;; If TRUE-PART does not end with unconditional jump, we
		;; have to jump to the end of FALSE-PART from here.
		(setq end-true-part-address ccl-current-ic)
		(ccl-embed-code 'jump 0 0)))
	  ;; This is the place to jump to if CONDITION is false.
	  (ccl-embed-current-address jump-cond-address)
	  ;; Compile FALSE-PART.
	  (setq unconditional-jump
		(and (ccl-compile-1 false-cmds) unconditional-jump))
	  (if end-true-part-address
	      ;; This is the place to jump to after the end of TRUE-PART.
	      (ccl-embed-current-address end-true-part-address))))
      unconditional-jump)))

;; Compile BRANCH statement.
(defun ccl-compile-branch (cmd)
  (if (< (length cmd) 3)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (ccl-compile-branch-blocks 'branch
			     (ccl-compile-branch-expression (nth 1 cmd) cmd)
			     (cdr (cdr cmd))))

;; Compile READ statement of the form `(read-branch EXPR BLOCK0 BLOCK1 ...)'.
(defun ccl-compile-read-branch (cmd)
  (if (< (length cmd) 3)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (ccl-compile-branch-blocks 'read-branch
			     (ccl-compile-branch-expression (nth 1 cmd) cmd)
			     (cdr (cdr cmd))))

;; Compile EXPRESSION part of BRANCH statement and return register
;; which holds a value of the expression.
(defun ccl-compile-branch-expression (expr cmd)
  (if (listp expr)
      ;; EXPR has the form `(EXPR2 OP ARG)'.  Compile it as SET
      ;; statement of the form `(r7 = (EXPR2 OP ARG))'.
      (progn
	(ccl-compile-expression 'r7 expr)
	'r7)
    (ccl-check-register expr cmd)))

;; Compile BLOCKs of BRANCH statement.  CODE is 'branch or 'read-branch.
;; REG is a register which holds a value of EXPRESSION part.  BLOCKs
;; is a list of CCL-BLOCKs.
(defun ccl-compile-branch-blocks (code rrr blocks)
  (let ((branches (length blocks))
	branch-idx
	jump-table-head-address
	empty-block-indexes
	block-tail-addresses
	block-unconditional-jump)
    (ccl-embed-code code rrr branches)
    (setq jump-table-head-address ccl-current-ic)
    ;; The size of jump table is the number of blocks plus 1 (for the
    ;; case RRR is out of range).
    (ccl-increment-ic (1+ branches))
    (setq empty-block-indexes (list branches))
    ;; Compile each block.
    (setq branch-idx 0)
    (while blocks
      (if (null (car blocks))
	  ;; This block is empty.
	  (setq empty-block-indexes (cons branch-idx empty-block-indexes)
		block-unconditional-jump t)
	;; This block is not empty.
	(ccl-embed-data (- ccl-current-ic jump-table-head-address)
			(+ jump-table-head-address branch-idx))
	(setq block-unconditional-jump (ccl-compile-1 (car blocks)))
	(if (not block-unconditional-jump)
	    (progn
	      ;; Jump address of the end of branches are embedded later.
	      ;; For the moment, just remember where to embed them.
	      (setq block-tail-addresses
		    (cons ccl-current-ic block-tail-addresses))
	      (ccl-embed-code 'jump 0 0))))
      (setq branch-idx (1+ branch-idx))
      (setq blocks (cdr blocks)))
    (if (not block-unconditional-jump)
	;; We don't need jump code at the end of the last block.
	(setq block-tail-addresses (cdr block-tail-addresses)
	      ccl-current-ic (1- ccl-current-ic)))
    ;; Embed jump address at the tailing jump commands of blocks.
    (while block-tail-addresses
      (ccl-embed-current-address (car block-tail-addresses))
      (setq block-tail-addresses (cdr block-tail-addresses)))
    ;; For empty blocks, make entries in the jump table point directly here.
    (while empty-block-indexes
      (ccl-embed-data (- ccl-current-ic jump-table-head-address)
		      (+ jump-table-head-address (car empty-block-indexes)))
      (setq empty-block-indexes (cdr empty-block-indexes))))
  ;; Branch command ends by unconditional jump if RRR is out of range.
  nil)

;; Compile LOOP statement.
(defun ccl-compile-loop (cmd)
  (if (< (length cmd) 2)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let* ((ccl-loop-head ccl-current-ic)
	 (ccl-breaks nil)
	 unconditional-jump)
    (setq cmd (cdr cmd))
    (if cmd
	(progn
	  (setq unconditional-jump t)
	  (while cmd
	    (setq unconditional-jump
		  (and (ccl-compile-1 (car cmd)) unconditional-jump))
	    (setq cmd (cdr cmd)))
	  (if (not ccl-breaks)
	      unconditional-jump
	    ;; Embed jump address for break statements encountered in
	    ;; this loop.
	    (while ccl-breaks
	      (ccl-embed-current-address (car ccl-breaks))
	      (setq ccl-breaks (cdr ccl-breaks))))
	  nil))))

;; Compile BREAK statement.
(defun ccl-compile-break (cmd)
  (if (/= (length cmd) 1)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (if (null ccl-loop-head)
      (error "CCL: No outer loop: %s" cmd))
  (setq ccl-breaks (cons ccl-current-ic ccl-breaks))
  (ccl-embed-code 'jump 0 0)
  t)

;; Compile REPEAT statement.
(defun ccl-compile-repeat (cmd)
  (if (/= (length cmd) 1)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (if (null ccl-loop-head)
      (error "CCL: No outer loop: %s" cmd))
  (ccl-embed-code 'jump 0 ccl-loop-head)
  t)

;; Compile WRITE-REPEAT statement.
(defun ccl-compile-write-repeat (cmd)
  (if (/= (length cmd) 2)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (if (null ccl-loop-head)
      (error "CCL: No outer loop: %s" cmd))
  (let ((arg (nth 1 cmd)))
    (cond ((integerp arg)
	   (ccl-embed-code 'write-const-jump 0 ccl-loop-head)
	   (ccl-embed-data arg))
	  ((stringp arg)
	   (let ((len (length arg))
		 (i 0))
	     (ccl-embed-code 'write-string-jump 0 ccl-loop-head)
	     (ccl-embed-data len)
	     (ccl-embed-string len arg)))
	  (t
	   (ccl-check-register arg cmd)
	   (ccl-embed-code 'write-register-jump arg ccl-loop-head))))
  t)

;; Compile WRITE-READ-REPEAT statement.
(defun ccl-compile-write-read-repeat (cmd)
  (if (or (< (length cmd) 2) (> (length cmd) 3))
      (error "CCL: Invalid number of arguments: %s" cmd))
  (if (null ccl-loop-head)
      (error "CCL: No outer loop: %s" cmd))
  (let ((rrr (ccl-check-register (nth 1 cmd) cmd))
	(arg (nth 2 cmd)))
    (cond ((null arg)
	   (ccl-embed-code 'write-register-read-jump rrr ccl-loop-head))
	  ((integerp arg)
	   (ccl-embed-code 'write-const-read-jump rrr arg ccl-loop-head))
	  ((vectorp arg)
	   (let ((len (length arg))
		 (i 0))
	     (ccl-embed-code 'write-array-read-jump rrr ccl-loop-head)
	     (ccl-embed-data len)
	     (while (< i len)
	       (ccl-embed-data (aref arg i))
	       (setq i (1+ i)))))
	  (t
	   (error "CCL: Invalid argument %s: %s" arg cmd)))
    (ccl-embed-code 'read-jump rrr ccl-loop-head))
  t)
			    
;; Compile READ statement.
(defun ccl-compile-read (cmd)
  (if (< (length cmd) 2)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let* ((args (cdr cmd))
	 (i (1- (length args))))
    (while args
      (let ((rrr (ccl-check-register (car args) cmd)))
	(ccl-embed-code 'read-register rrr i)
	(setq args (cdr args) i (1- i)))))
  nil)

;; Compile READ-IF statement.
(defun ccl-compile-read-if (cmd)
  (ccl-compile-if cmd 'read))

;; Compile WRITE statement.
(defun ccl-compile-write (cmd)
  (if (< (length cmd) 2)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((rrr (nth 1 cmd)))
    (cond ((integerp rrr)
	   (ccl-embed-code 'write-const-string 0 rrr))
	  ((stringp rrr)
	   (ccl-compile-write-string rrr))
	  ((and (symbolp rrr) (vectorp (nth 2 cmd)))
	   (ccl-check-register rrr cmd)
	   ;; CMD has the form `(write REG ARRAY)'.
	   (let* ((arg (nth 2 cmd))
		  (len (length arg))
		  (i 0))
	     (ccl-embed-code 'write-array rrr len)
	     (while (< i len)
	       (if (not (integerp (aref arg i)))
		   (error "CCL: Invalid argument %s: %s" arg cmd))
	       (ccl-embed-data (aref arg i))
	       (setq i (1+ i)))))

	  ((symbolp rrr)
	   ;; CMD has the form `(write REG ...)'.
	   (let* ((args (cdr cmd))
		  (i (1- (length args))))
	     (while args
	       (setq rrr (ccl-check-register (car args) cmd))
	       (ccl-embed-code 'write-register rrr i)
	       (setq args (cdr args) i (1- i)))))

	  ((listp rrr)
	   ;; CMD has the form `(write (LEFT OP RIGHT))'.
	   (let ((left (car rrr))
		 (op (get (nth 1 rrr) 'ccl-arith-code))
		 (right (nth 2 rrr)))
	     (if (listp left)
		 (progn
		   ;; RRR has the form `((EXPR OP2 ARG) OP RIGHT)'.
		   ;; Compile the first term as `(r7 = (EXPR OP2 ARG))'.
		   (ccl-compile-expression 'r7 left)
		   (setq left 'r7)))
	     ;; Now RRR has the form `(ARG OP RIGHT)'.
	     (if (integerp right)
		 (progn
		   (ccl-embed-code 'write-expr-const 0 (ash op 3) left)
		   (ccl-embed-data right))
	       (ccl-check-register right rrr)
	       (ccl-embed-code 'write-expr-register 0
			       (logior (ash op 3)
				       (get right 'ccl-register-number))))))

	  (t
	   (error "CCL: Invalid argument: %s" cmd))))
  nil)

;; Compile CALL statement.
(defun ccl-compile-call (cmd)
  (if (/= (length cmd) 2)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (if (not (symbolp (nth 1 cmd)))
      (error "CCL: Subroutine should be a symbol: %s" cmd))
  (let* ((name (nth 1 cmd))
	 (idx (get name 'ccl-program-idx)))
    (if (not idx)
	(error "CCL: Unknown subroutine name: %s" name))
    (ccl-embed-code 'call 0 idx))
  nil)

;; Compile END statement.
(defun ccl-compile-end (cmd)
  (if (/= (length cmd) 1)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (ccl-embed-code 'end 0 0)
  t)

;; Compile read-multibyte-character
(defun ccl-compile-read-multibyte-character (cmd)
  (if (/= (length cmd) 3)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((RRR (nth 1 cmd))
	(rrr (nth 2 cmd)))
    (ccl-check-register rrr cmd)
    (ccl-check-register RRR cmd)
    (ccl-embed-extended-command 'read-multibyte-character rrr RRR 0)))

;; Compile write-multibyte-character
(defun ccl-compile-write-multibyte-character (cmd)
  (if (/= (length cmd) 3)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((RRR (nth 1 cmd))
	(rrr (nth 2 cmd)))
    (ccl-check-register rrr cmd)
    (ccl-check-register RRR cmd)
    (ccl-embed-extended-command 'write-multibyte-character rrr RRR 0)))

;; Compile translate-character
(defun ccl-compile-translate-character (cmd)
  (if (/= (length cmd) 4)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((Rrr (nth 1 cmd))
	(RRR (nth 2 cmd))
	(rrr (nth 3 cmd)))
    (ccl-check-register rrr cmd)
    (ccl-check-register RRR cmd)
    (cond ((symbolp Rrr)
	   (if (not (get Rrr 'translation-table))
	       (error "CCL: Invalid translation table %s in %s" Rrr cmd))
	   (ccl-embed-extended-command 'translate-character-const-tbl
				       rrr RRR 0)
	   (ccl-embed-data Rrr))
	  (t
	   (ccl-check-register Rrr cmd)
	   (ccl-embed-extended-command 'translate-character rrr RRR Rrr)))))

(defun ccl-compile-iterate-multiple-map (cmd)
  (ccl-compile-multiple-map-function 'iterate-multiple-map cmd))

(defun ccl-compile-map-multiple (cmd)
  (if (/= (length cmd) 4)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((func '(lambda (arg mp)
			  (let ((len 0) result add)
			    (while arg
			      (if (consp (car arg))
				  (setq add (funcall func (car arg) t)
					result (append result add)
					add (+ (-(car add)) 1))
				(setq result
				      (append result
					      (list (car arg)))
				      add 1))
			      (setq arg (cdr arg)
				    len (+ len add)))
			    (if mp 
				(cons (- len) result)
			      result))))
	arg)
    (setq arg (append (list (nth 0 cmd) (nth 1 cmd) (nth 2 cmd))
		      (funcall func (nth 3 cmd) nil)))
    (ccl-compile-multiple-map-function 'map-multiple arg)))

(defun ccl-compile-map-single (cmd)
  (if (/= (length cmd) 4)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((RRR (nth 1 cmd))
	(rrr (nth 2 cmd))
	(map (nth 3 cmd))
	id)
    (ccl-check-register rrr cmd)
    (ccl-check-register RRR cmd)
    (ccl-embed-extended-command 'map-single rrr RRR 0)
    (cond ((symbolp map)
	   (if (get map 'code-conversion-map)
	       (ccl-embed-data map)
	     (error "CCL: Invalid map: %s" map)))
	  (t
	   (error "CCL: Invalid type of arguments: %s" cmd)))))

(defun ccl-compile-multiple-map-function (command cmd)
  (if (< (length cmd) 4)
      (error "CCL: Invalid number of arguments: %s" cmd))
  (let ((RRR (nth 1 cmd))
	(rrr (nth 2 cmd))
	(args (nthcdr 3 cmd))
	map)
    (ccl-check-register rrr cmd)
    (ccl-check-register RRR cmd)
    (ccl-embed-extended-command command rrr RRR 0)
    (ccl-embed-data (length args))
    (while args
      (setq map (car args))
      (cond ((symbolp map)
	     (if (get map 'code-conversion-map)
		 (ccl-embed-data map)
	       (error "CCL: Invalid map: %s" map)))
	    ((numberp map)
	     (ccl-embed-data map))
	    (t
	     (error "CCL: Invalid type of arguments: %s" cmd)))
      (setq args (cdr args)))))


;;; CCL dump staffs

;; To avoid byte-compiler warning.
(defvar ccl-code)

;;;###autoload
(defun ccl-dump (ccl-code)
  "Disassemble compiled CCL-CODE."
  (let ((len (length ccl-code))
	(buffer-mag (aref ccl-code 0)))
    (cond ((= buffer-mag 0)
	   (insert "Don't output anything.\n"))
	  ((= buffer-mag 1)
	   (insert "Out-buffer must be as large as in-buffer.\n"))
	  (t
	   (insert
	    (format "Out-buffer must be %d times bigger than in-buffer.\n"
		    buffer-mag))))
    (insert "Main-body:\n")
    (setq ccl-current-ic 2)
    (if (> (aref ccl-code 1) 0)
	(progn
	  (while (< ccl-current-ic (aref ccl-code 1))
	    (ccl-dump-1))
	  (insert "At EOF:\n")))
    (while (< ccl-current-ic len)
      (ccl-dump-1))
    ))

;; Return a CCL code in `ccl-code' at `ccl-current-ic'.
(defun ccl-get-next-code ()
  (prog1
      (aref ccl-code ccl-current-ic)
    (setq ccl-current-ic (1+ ccl-current-ic))))

(defun ccl-dump-1 ()
  (let* ((code (ccl-get-next-code))
	 (cmd (aref ccl-code-table (logand code 31)))
	 (rrr (ash (logand code 255) -5))
	 (cc (ash code -8)))
    (insert (format "%5d:[%s] " (1- ccl-current-ic) cmd))
    (funcall (get cmd 'ccl-dump-function) rrr cc))) 

(defun ccl-dump-set-register (rrr cc)
  (insert (format "r%d = r%d\n" rrr cc)))

(defun ccl-dump-set-short-const (rrr cc)
  (insert (format "r%d = %d\n" rrr cc)))

(defun ccl-dump-set-const (rrr ignore)
  (insert (format "r%d = %d\n" rrr (ccl-get-next-code))))

(defun ccl-dump-set-array (rrr cc)
  (let ((rrr2 (logand cc 7))
	(len (ash cc -3))
	(i 0))
    (insert (format "r%d = array[r%d] of length %d\n\t"
		    rrr rrr2 len))
    (while (< i len)
      (insert (format "%d " (ccl-get-next-code)))
      (setq i (1+ i)))
    (insert "\n")))

(defun ccl-dump-jump (ignore cc &optional address)
  (insert (format "jump to %d(" (+ (or address ccl-current-ic) cc)))
  (if (>= cc 0)
      (insert "+"))
  (insert (format "%d)\n" (1+ cc))))

(defun ccl-dump-jump-cond (rrr cc)
  (insert (format "if (r%d == 0), " rrr))
  (ccl-dump-jump nil cc))

(defun ccl-dump-write-register-jump (rrr cc)
  (insert (format "write r%d, " rrr))
  (ccl-dump-jump nil cc))

(defun ccl-dump-write-register-read-jump (rrr cc)
  (insert (format "write r%d, read r%d, " rrr rrr))
  (ccl-dump-jump nil cc)
  (ccl-get-next-code)			; Skip dummy READ-JUMP
  )

(defun ccl-extract-arith-op (cc)
  (aref ccl-arith-table (ash cc -6)))

(defun ccl-dump-write-expr-const (ignore cc)
  (insert (format "write (r%d %s %d)\n"
		  (logand cc 7)
		  (ccl-extract-arith-op cc)
		  (ccl-get-next-code))))

(defun ccl-dump-write-expr-register (ignore cc)
  (insert (format "write (r%d %s r%d)\n"
		  (logand cc 7)
		  (ccl-extract-arith-op cc)
		  (logand (ash cc -3) 7))))

(defun ccl-dump-insert-char (cc)
  (cond ((= cc ?\t) (insert " \"^I\""))
	((= cc ?\n) (insert " \"^J\""))
	(t (insert (format " \"%c\"" cc)))))

(defun ccl-dump-write-const-jump (ignore cc)
  (let ((address ccl-current-ic))
    (insert "write char")
    (ccl-dump-insert-char (ccl-get-next-code))
    (insert ", ")
    (ccl-dump-jump nil cc address)))

(defun ccl-dump-write-const-read-jump (rrr cc)
  (let ((address ccl-current-ic))
    (insert "write char")
    (ccl-dump-insert-char (ccl-get-next-code))
    (insert (format ", read r%d, " rrr))
    (ccl-dump-jump cc address)
    (ccl-get-next-code)			; Skip dummy READ-JUMP
    ))

(defun ccl-dump-write-string-jump (ignore cc)
  (let ((address ccl-current-ic)
	(len (ccl-get-next-code))
	(i 0))
    (insert "write \"")
    (while (< i len)
      (let ((code (ccl-get-next-code)))
	(insert (ash code -16))
	(if (< (1+ i) len) (insert (logand (ash code -8) 255)))
	(if (< (+ i 2) len) (insert (logand code 255))))
      (setq i (+ i 3)))
    (insert "\", ")
    (ccl-dump-jump nil cc address)))

(defun ccl-dump-write-array-read-jump (rrr cc)
  (let ((address ccl-current-ic)
	(len (ccl-get-next-code))
	(i 0))
    (insert (format "write array[r%d] of length %d,\n\t" rrr len))
    (while (< i len)
      (ccl-dump-insert-char (ccl-get-next-code))
      (setq i (1+ i)))
    (insert (format "\n\tthen read r%d, " rrr))
    (ccl-dump-jump nil cc address)
    (ccl-get-next-code)			; Skip dummy READ-JUMP.
    ))

(defun ccl-dump-read-jump (rrr cc)
  (insert (format "read r%d, " rrr))
  (ccl-dump-jump nil cc))

(defun ccl-dump-branch (rrr len)
  (let ((jump-table-head ccl-current-ic)
	(i 0))
    (insert (format "jump to array[r%d] of length %d\n\t" rrr len))
    (while (<= i len)
      (insert (format "%d " (+ jump-table-head (ccl-get-next-code))))
      (setq i (1+ i)))
    (insert "\n")))

(defun ccl-dump-read-register (rrr cc)
  (insert (format "read r%d (%d remaining)\n" rrr cc)))

(defun ccl-dump-read-branch (rrr len)
  (insert (format "read r%d, " rrr))
  (ccl-dump-branch rrr len))

(defun ccl-dump-write-register (rrr cc)
  (insert (format "write r%d (%d remaining)\n" rrr cc)))

(defun ccl-dump-call (ignore cc)
  (insert (format "call subroutine #%d\n" cc)))

(defun ccl-dump-write-const-string (rrr cc)
  (if (= rrr 0)
      (progn
	(insert "write char")
	(ccl-dump-insert-char cc)
	(newline))
    (let ((len cc)
	  (i 0))
      (insert "write \"")
      (while (< i len)
	(let ((code (ccl-get-next-code)))
	  (insert (format "%c" (lsh code -16)))
	  (if (< (1+ i) len)
	      (insert (format "%c" (logand (lsh code -8) 255))))
	  (if (< (+ i 2) len)
	      (insert (format "%c" (logand code 255))))
	  (setq i (+ i 3))))
      (insert "\"\n"))))

(defun ccl-dump-write-array (rrr cc)
  (let ((i 0))
    (insert (format "write array[r%d] of length %d\n\t" rrr cc))
    (while (< i cc)
      (ccl-dump-insert-char (ccl-get-next-code))
      (setq i (1+ i)))
    (insert "\n")))

(defun ccl-dump-end (&rest ignore)
  (insert "end\n"))

(defun ccl-dump-set-assign-expr-const (rrr cc)
  (insert (format "r%d %s= %d\n"
		  rrr
		  (ccl-extract-arith-op cc)
		  (ccl-get-next-code))))

(defun ccl-dump-set-assign-expr-register (rrr cc)
  (insert (format "r%d %s= r%d\n"
		  rrr
		  (ccl-extract-arith-op cc)
		  (logand cc 7))))

(defun ccl-dump-set-expr-const (rrr cc)
  (insert (format "r%d = r%d %s %d\n"
		  rrr
		  (logand cc 7)
		  (ccl-extract-arith-op cc)
		  (ccl-get-next-code))))

(defun ccl-dump-set-expr-register (rrr cc)
  (insert (format "r%d = r%d %s r%d\n"
		  rrr
		  (logand cc 7)
		  (ccl-extract-arith-op cc)
		  (logand (ash cc -3) 7))))

(defun ccl-dump-jump-cond-expr-const (rrr cc)
  (let ((address ccl-current-ic))
    (insert (format "if !(r%d %s %d), "
		    rrr
		    (aref ccl-arith-table (ccl-get-next-code))
		    (ccl-get-next-code)))
    (ccl-dump-jump nil cc address)))

(defun ccl-dump-jump-cond-expr-register (rrr cc)
  (let ((address ccl-current-ic))
    (insert (format "if !(r%d %s r%d), "
		    rrr
		    (aref ccl-arith-table (ccl-get-next-code))
		    (ccl-get-next-code)))
    (ccl-dump-jump nil cc address)))

(defun ccl-dump-read-jump-cond-expr-const (rrr cc)
  (insert (format "read r%d, " rrr))
  (ccl-dump-jump-cond-expr-const rrr cc))

(defun ccl-dump-read-jump-cond-expr-register (rrr cc)
  (insert (format "read r%d, " rrr))
  (ccl-dump-jump-cond-expr-register rrr cc))

(defun ccl-dump-binary (ccl-code)
  (let ((len (length ccl-code))
	(i 2))
    (while (< i len)
      (let ((code (aref ccl-code i))
	    (j 27))
	(while (>= j 0)
	  (insert (if (= (logand code (ash 1 j)) 0) ?0 ?1))
	  (setq j (1- j)))
	(setq code (logand code 31))
	(if (< code (length ccl-code-table))
	    (insert (format ":%s" (aref ccl-code-table code))))
	(insert "\n"))
      (setq i (1+ i)))))

(defun ccl-dump-ex-cmd (rrr cc)
  (let* ((RRR (logand cc ?\x7))
	 (Rrr (logand (ash cc -3) ?\x7))
	 (ex-op (aref ccl-extended-code-table (logand (ash cc -6) ?\x3fff))))
    (insert (format "<%s> " ex-op))
    (funcall (get ex-op 'ccl-dump-function) rrr RRR Rrr)))

(defun ccl-dump-read-multibyte-character (rrr RRR Rrr)
  (insert (format "read-multibyte-character r%d r%d\n" RRR rrr)))

(defun ccl-dump-write-multibyte-character (rrr RRR Rrr)
  (insert (format "write-multibyte-character r%d r%d\n" RRR rrr)))

(defun ccl-dump-translate-character (rrr RRR Rrr)
  (insert (format "translation table(r%d) r%d r%d\n" Rrr RRR rrr)))

(defun ccl-dump-translate-character-const-tbl (rrr RRR Rrr)
  (let ((tbl (ccl-get-next-code)))
    (insert (format "translation table(%d) r%d r%d\n" tbl RRR rrr))))

(defun ccl-dump-iterate-multiple-map (rrr RRR Rrr)
  (let ((notbl (ccl-get-next-code))
	(i 0) id)
    (insert (format "iterate-multiple-map r%d r%d\n" RRR rrr))
    (insert (format "\tnumber of maps is %d .\n\t [" notbl))
    (while (< i notbl)
      (setq id (ccl-get-next-code))
      (insert (format "%S" id))
      (setq i (1+ i)))
    (insert "]\n")))

(defun ccl-dump-map-multiple (rrr RRR Rrr)
  (let ((notbl (ccl-get-next-code))
	(i 0) id)
    (insert (format "map-multiple r%d r%d\n" RRR rrr))
    (insert (format "\tnumber of maps and separators is %d\n\t [" notbl))
    (while (< i notbl)
      (setq id (ccl-get-next-code))
      (if (= id -1)
	  (insert "]\n\t [")
	(insert (format "%S " id)))
      (setq i (1+ i)))
    (insert "]\n")))

(defun ccl-dump-map-single (rrr RRR Rrr)
  (let ((id (ccl-get-next-code)))
    (insert (format "map-single r%d r%d map(%S)\n" RRR rrr id))))


;; CCL emulation staffs 

;; Not yet implemented.

;; Auto-loaded functions.

;;;###autoload
(defmacro declare-ccl-program (name &optional vector)
  "Declare NAME as a name of CCL program.

To compile a CCL program which calls another CCL program not yet
defined, it must be declared as a CCL program in advance.
Optional arg VECTOR is a compiled CCL code of the CCL program."
  `(put ',name 'ccl-program-idx (register-ccl-program ',name ,vector)))

;;;###autoload
(defmacro define-ccl-program (name ccl-program &optional doc)
  "Set NAME the compiled code of CCL-PROGRAM.
CCL-PROGRAM is `eval'ed before being handed to the CCL compiler `ccl-compile'.
The compiled code is a vector of integers."
  `(let ((prog ,(ccl-compile (eval ccl-program))))
     (defconst ,name prog ,doc)
     (put ',name 'ccl-program-idx (register-ccl-program ',name prog))
     nil))

;;;###autoload
(defmacro check-ccl-program (ccl-program &optional name)
  "Check validity of CCL-PROGRAM.
If CCL-PROGRAM is a symbol denoting a valid CCL program, return
CCL-PROGRAM, else return nil.
If CCL-PROGRAM is a vector and optional arg NAME (symbol) is supplied,
register CCL-PROGRAM by name NAME, and return NAME."
  `(let ((result ,ccl-program))
     (cond ((symbolp ,ccl-program)
	    (or (numberp (get ,ccl-program 'ccl-program-idx))
		(setq result nil)))
	   ((vectorp ,ccl-program)
	    (setq result ,name)
	    (register-ccl-program result ,ccl-program))
	   (t
	    (setq result nil)))
     result))

;;;###autoload
(defun ccl-execute-with-args (ccl-prog &rest args)
  "Execute CCL-PROGRAM with registers initialized by the remaining args.
The return value is a vector of resulting CCL registeres."
  (let ((reg (make-vector 8 0))
	(i 0))
    (while (and args (< i 8))
      (if (not (integerp (car args)))
	  (error "Arguments should be integer"))
      (aset reg i (car args))
      (setq args (cdr args) i (1+ i)))
    (ccl-execute ccl-prog reg)
    reg))

(provide 'ccl)

;; ccl.el ends here
