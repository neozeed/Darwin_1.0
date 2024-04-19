;;; net-utils.el --- Network functions

;; Author:  Peter Breton <pbreton@cs.umb.edu>
;; Created: Sun Mar 16 1997
;; Keywords: network communications
;; Time-stamp: <1998-06-13 06:19:01 pbreton>

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
;; There are three main areas of functionality:
;; 
;; * Wrap common network utility programs (ping, traceroute, netstat,
;; nslookup, arp, route). Note that these wrappers are of the diagnostic
;; functions of these programs only.
;; 
;; * Implement some very basic protocols in Emacs Lisp (finger and whois)
;; 
;; * Support connections to HOST/PORT, generally for debugging and the like.
;; In other words, for doing much the same thing as "telnet HOST PORT", and
;; then typing commands.
;;
;; PATHS
;;
;; On some systems, some of these programs are not in normal user path,
;; but rather in /sbin, /usr/sbin, and so on. 


;;; Code:
(eval-when-compile
  (require 'comint)
  (require 'ffap))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Customization Variables
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defgroup net-utils nil
  "Network utility functions."
  :prefix "net-utils-"
  :group 'comm
  :version "20.3"
  )

(defcustom net-utils-remove-ctl-m 
  (member system-type (list 'windows-nt 'msdos))
  "If non-nil, remove control-Ms from output."
  :group 'net-utils
  :type  'boolean
  )

(defcustom traceroute-program  
  (if (eq system-type 'windows-nt) 
      "tracert"
    "traceroute")
  "Program to trace network hops to a destination."
  :group 'net-utils
  :type  'string
  )

(defcustom traceroute-program-options nil
  "Options for the traceroute program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom ping-program "ping"
  "Program to send network test packets to a host."
  :group 'net-utils
  :type  'string
  )

;; On Linux and Irix, the system's ping program seems to send packets
;; indefinitely unless told otherwise
(defcustom ping-program-options 
  (and (memq system-type (list 'linux 'gnu/linux 'irix))
       (list "-c" "4"))
  "Options for the ping program.
These options can be used to limit how many ICMP packets are emitted."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom ipconfig-program  
  (if (eq system-type 'windows-nt)
      "ipconfig"
    "ifconfig")
  "Program to print network configuration information."
  :group 'net-utils
  :type  'string
  )

(defcustom ipconfig-program-options
   (list    
    (if (eq system-type 'windows-nt)
	"/all" "-a"))
  "Options for ipconfig-program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom netstat-program  "netstat"
  "Program to print network statistics."
  :group 'net-utils
  :type  'string
  )

(defcustom netstat-program-options nil
  "Options for netstat-program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom arp-program  "arp"
  "Program to print IP to address translation tables."
  :group 'net-utils
  :type  'string
  )

(defcustom arp-program-options 
  (list "-a")
  "Options for arp-program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom route-program  
  (if (eq system-type 'windows-nt)
      "route"
    "netstat")
  "Program to print routing tables."
  :group 'net-utils
  :type  'string
  )

(defcustom route-program-options 
  (if (eq system-type 'windows-nt)
      (list "print")
    (list "-r"))
  "Options for route-program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom nslookup-program  "nslookup"
  "Program to interactively query DNS information."
  :group 'net-utils
  :type  'string
  )

(defcustom nslookup-program-options  nil
  "List of options to pass to the nslookup program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom nslookup-prompt-regexp "^> "
  "Regexp to match the nslookup prompt."
  :group 'net-utils
  :type  'regexp
  )

(defcustom ftp-program "ftp"
  "Progam to run to do FTP transfers."
  :group 'net-utils
  :type  'string
  )

(defcustom ftp-program-options nil
  "List of options to pass to the FTP program."
  :group 'net-utils
  :type  '(repeat 'string)
  )

(defcustom ftp-prompt-regexp "^ftp>"
  "Regexp which matches the FTP program's prompt."
  :group 'net-utils
  :type  'regexp
  )

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Nslookup goodies
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defconst nslookup-font-lock-keywords
  (and window-system
       (progn
	 (require 'font-lock)
	 (list
	  (list nslookup-prompt-regexp 0 font-lock-reference-face)
	  (list "^[A-Za-z0-9 _]+:"     0 font-lock-type-face)
	  (list "\\<\\(SOA\\|NS\\|MX\\|A\\|CNAME\\)\\>" 
		1 font-lock-keyword-face)
	  ;; Dotted quads
	  (list 
	   (mapconcat 'identity
		      (make-list 4 "[0-9]+")
		      "\\.")
	   0 font-lock-variable-name-face)
	  ;; Host names
	  (list 
	   (let ((host-expression "[-A-Za-z0-9]+"))
	     (concat 
	      (mapconcat 'identity
			 (make-list 2 host-expression)
			 "\\.")
	      "\\(\\." host-expression "\\)*")
	     )
	   0 font-lock-variable-name-face)
	  )))
	 "Expressions to font-lock for nslookup.")

(defvar nslookup-abbrev-table (make-abbrev-table)
  "Abbrev table for nslookup.")

(define-abbrev nslookup-abbrev-table "e"   "exit")
(define-abbrev nslookup-abbrev-table "f"   "finger")
(define-abbrev nslookup-abbrev-table "h"   "help")
(define-abbrev nslookup-abbrev-table "lse" "lserver")
(define-abbrev nslookup-abbrev-table "r"   "root")
(define-abbrev nslookup-abbrev-table "s"   "set")
(define-abbrev nslookup-abbrev-table "se"  "server")
(define-abbrev nslookup-abbrev-table "v"   "viewer")

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; FTP goodies
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defvar ftp-abbrev-table (make-abbrev-table)
  "Abbrev table for ftp.")

(define-abbrev ftp-abbrev-table "q"    "quit")
(define-abbrev ftp-abbrev-table "g"    "get")
(define-abbrev ftp-abbrev-table "p"    "prompt")
(define-abbrev ftp-abbrev-table "anon" "anonymous")

(defconst ftp-font-lock-keywords
  (and window-system
       (progn
	 (require 'font-lock)
	 (list
	  (list ftp-prompt-regexp 0 font-lock-reference-face)))))


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Utility functions
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(defun net-utils-remove-ctrl-m-filter (process output-string)
  "Remove trailing control Ms."
  (let ((old-buffer (current-buffer))
	(filtered-string output-string))
    (unwind-protect
	(let ((moving))
	  (set-buffer (process-buffer process))
	  (setq moving (= (point) (process-mark process)))
	  
	  (while (string-match "\r" filtered-string)
	       (setq filtered-string
		     (replace-match "" nil nil filtered-string)))

	  (save-excursion
	    ;; Insert the text, moving the process-marker.
	    (goto-char (process-mark process))
	    (insert filtered-string)
	    (set-marker (process-mark process) (point)))
	  (if moving (goto-char (process-mark process))))
      (set-buffer old-buffer))))
  
(defmacro net-utils-run-program (name header program &rest args)
  "Run a network information program."
  (` 
   (let ((buf (get-buffer-create (concat "*" (, name) "*"))))
     (set-buffer buf)
     (erase-buffer)
     (insert (, header) "\n")
     (set-process-filter 
      (apply 'start-process (, name) buf (, program) (,@ args))
      'net-utils-remove-ctrl-m-filter)
     (display-buffer buf))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Wrappers for external network programs
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;###autoload
(defun traceroute (target)
  "Run traceroute program for TARGET."
  (interactive "sTarget: ")
  (let ((options 
	 (if traceroute-program-options
	     (append traceroute-program-options (list target))
	   (list target))))
    (net-utils-run-program
     (concat "Traceroute" " " target)
     (concat "** Traceroute ** " traceroute-program " ** " target)
     traceroute-program
     options
     )))

;;;###autoload
(defun ping (host)
  "Ping HOST.
If your system's ping continues until interrupted, you can try setting 
`ping-program-options'."
  (interactive 
   (list
    (progn
      (require 'ffap)
      (read-from-minibuffer 
       "Ping host: " 
       (or (ffap-string-at-point 'machine) "")))))
  (let ((options 
	 (if ping-program-options
	     (append ping-program-options (list host))
	   (list host))))
    (net-utils-run-program
     (concat "Ping" " " host)
     (concat "** Ping ** " ping-program " ** " host)
     ping-program
     options
     )))

;;;###autoload
(defun ipconfig ()
  "Run ipconfig program."
  (interactive)
  (net-utils-run-program
   "Ipconfig"
   (concat "** Ipconfig ** " ipconfig-program " ** ")
   ipconfig-program
   ipconfig-program-options
   ))

;; This is the normal name on most Unixes.
;;;###autoload
(defalias 'ifconfig 'ipconfig) 

;;;###autoload
(defun netstat ()
  "Run netstat program."
  (interactive)
  (net-utils-run-program
   "Netstat"
   (concat "** Netstat ** " netstat-program " ** ")
   netstat-program
   netstat-program-options
   ))

;;;###autoload
(defun arp ()
  "Run the arp program."
  (interactive)
  (net-utils-run-program
   "Arp"
   (concat "** Arp ** " arp-program " ** ")
   arp-program
   arp-program-options
   ))

;;;###autoload
(defun route ()
  "Run the route program."
  (interactive)
  (net-utils-run-program
   "Route"
   (concat "** Route ** " route-program " ** ")
   route-program
   route-program-options
   ))

;; FIXME -- Needs to be a process filter
;; (defun netstat-with-filter (filter)
;;   "Run netstat program."
;;   (interactive "sFilter: ")
;;   (netstat)
;;   (set-buffer (get-buffer "*Netstat*"))
;;   (goto-char (point-min))
;;   (delete-matching-lines filter)
;;   )

;;;###autoload
(defun nslookup-host (host)
  "Lookup the DNS information for HOST."
  (interactive
   (list
    (read-from-minibuffer 
     "Lookup host: " 
     (or (ffap-string-at-point 'machine) ""))))
  (let ((options 
	 (if nslookup-program-options
	     (append nslookup-program-options (list host))
	   (list host))))
    (net-utils-run-program
     "Nslookup"
     (concat "** "
      (mapconcat 'identity
		(list "Nslookup" host nslookup-program)
		" ** "))
     nslookup-program
     options
     )))


;;;###autoload
(defun nslookup ()
  "Run nslookup program."
  (interactive)
  (comint-run nslookup-program)
  (set-process-filter (get-buffer-process "*nslookup*")
   'net-utils-remove-ctrl-m-filter)
  (set 
   (make-local-variable 'font-lock-defaults)
   '((nslookup-font-lock-keywords)))
  (set 
   (make-local-variable 'local-abbrev-table)
   nslookup-abbrev-table)
  (abbrev-mode t)
  (make-local-variable 'comint-prompt-regexp)
  (setq comint-prompt-regexp nslookup-prompt-regexp)
  )

;; This is a lot less than ange-ftp, but much simpler.
;;;###autoload
(defun ftp (host)
  "Run ftp program."
  (interactive "sFtp to Host: ")
  (let ((buf (get-buffer-create (concat "*ftp [" host "]*"))))
    (set-buffer buf)
    (comint-mode)
    (comint-exec buf (concat "ftp-" host) ftp-program nil
		 (if ftp-program-options
		     (append (list host) ftp-program-options)
		   (list host)))
    (set 
     (make-local-variable 'font-lock-defaults)
     '((ftp-font-lock-keywords)))

    (make-local-variable 'comint-prompt-regexp)
    (setq comint-prompt-regexp ftp-prompt-regexp)

    ;; Already buffer local!
    (setq comint-output-filter-functions
	  (list 'comint-watch-for-password-prompt))
    (set 
     (make-local-variable 'local-abbrev-table)
     ftp-abbrev-table)
    (abbrev-mode t)
    (switch-to-buffer-other-window buf)
    ))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Network Connections
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Full list is available at:
;; ftp://ftp.isi.edu/in-notes/iana/assignments/port-numbers
(defvar network-connection-service-alist 
  (list
    (cons 'echo          7)
    (cons 'active-users 11)
    (cons 'daytime      13)
    (cons 'chargen      19)
    (cons 'ftp          21)
    (cons 'telnet	23)
    (cons 'smtp		25)
    (cons 'time		37)
    (cons 'whois        43)
    (cons 'gopher       70)
    (cons 'finger       79)
    (cons 'www		80)
    (cons 'pop2		109)
    (cons 'pop3		110)
    (cons 'sun-rpc	111)
    (cons 'nntp		119)
    (cons 'ntp		123)
    (cons 'netbios-name 137)
    (cons 'netbios-data 139)
    (cons 'irc		194)
    (cons 'https	443)
    (cons 'rlogin	513)
    )
  "Alist of services and associated TCP port numbers.
This list in not complete.")

;; Workhorse macro
(defmacro run-network-program (process-name host port 
					    &optional initial-string)
  (`
   (let ((tcp-connection)
	 (buf)
	 )
    (setq buf (get-buffer-create (concat "*" (, process-name) "*")))
    (set-buffer buf)
    (or 
     (setq tcp-connection
	   (open-network-stream 
	    (, process-name)
	    buf
	    (, host)
	    (, port)
	    ))
     (error "Could not open connection to %s" (, host)))
    (erase-buffer)
    (set-marker (process-mark tcp-connection) (point-min))
    (set-process-filter tcp-connection 'net-utils-remove-ctrl-m-filter)
    (and (, initial-string)
	 (process-send-string tcp-connection 
			      (concat (, initial-string) "\r\n")))
    (display-buffer buf))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Simple protocols
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; Finger protocol
;;;###autoload
(defun finger (user host)
  "Finger USER on HOST."
  ;; One of those great interactive statements that's actually
  ;; longer than the function call! The idea is that if the user
  ;; uses a string like "pbreton@cs.umb.edu", we won't ask for the
  ;; host name. If we don't see an "@", we'll prompt for the host.
  (interactive
    (progn
      (require 'ffap)
      (let* ((answer (read-from-minibuffer "Finger User: " 
					   (ffap-string-at-point 'url))) 
	     (index  (string-match (regexp-quote "@") answer))
	     )
	(if index
	    (list 
	     (substring answer 0 index)
	     (substring answer (1+ index)))
	  (list
	   answer
	   (read-from-minibuffer 
	    "At Host: " 
	    (or (ffap-string-at-point 'machine) "")))))))
  (let* (
	 (user-and-host (concat user "@" host))
	 (process-name 
	  (concat "Finger [" user-and-host "]"))
	 )
    (run-network-program 
     process-name 
     host 
     (cdr (assoc 'finger network-connection-service-alist))
     user-and-host
     )))

(defcustom whois-server-name "whois.internic.net"
  "Host name for the whois service."
  :group 'net-utils
  :type  'string
  )

;; Whois protocol
;;;###autoload
(defun whois (arg search-string)
  "Send SEARCH-STRING to server defined by the `whois-server-name' variable.
With argument, prompt for whois server."
  (interactive "P\nsWhois: ")
  (let ((host 
	 (if arg
	     (read-from-minibuffer "Whois server name: ")
	   whois-server-name))
	)
    (run-network-program 
     "Whois"
     host
     (cdr (assoc 'whois network-connection-service-alist))
     search-string
     )))

(defcustom whois-reverse-lookup-server "whois.arin.net"
  "Server which provides inverse DNS mapping."
  :group 'net-utils
  :type  'string
  )

;;;###autoload
(defun whois-reverse-lookup ()
  (interactive)
  (let ((whois-server-name whois-reverse-lookup-server))
    (call-interactively 'whois)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;; General Network connection
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;;;###autoload
(defun network-connection-to-service (host service)
  "Open a network connection to SERVICE on HOST."
  (interactive 
   (list
    (progn
      (require 'ffap)
      (read-from-minibuffer "Host: " 
			    (ffap-string-at-point 'machine)))
    (completing-read "Service: " 
		     (mapcar 
		      (function 
		       (lambda (elt)
			 (list (symbol-name (car elt)))))
		      network-connection-service-alist))))
  (network-connection 
   host 
   (cdr (assoc (intern service) network-connection-service-alist)))
  )

;;;###autoload
(defun network-connection (host port)
  "Open a network connection to HOST on PORT."
  (interactive "sHost: \nnPort: ")
  (network-service-connection host (number-to-string port)))

(defun network-service-connection (host service)
  "Open a network connection to SERVICE on HOST."
  (let (
	(process-name (concat "Network Connection [" host " " service "]"))
	(portnum (string-to-number service))
	)
    (or (zerop portnum) (setq service portnum))
    (make-comint 
     process-name
     (cons host service))
    (pop-to-buffer (get-buffer (concat "*" process-name "*")))
    ))

(provide 'net-utils)

;;; net-utils.el ends here
