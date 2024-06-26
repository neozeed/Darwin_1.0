;;;#ident "@(#)OrigId: pcl-cvs-startup.el,v 1.4 1993/05/31 18:40:33 ceder Exp "
;;;
;;;#ident "@(#)cvs/contrib/pcl-cvs:$Name: cvs-16 $:$Id: pcl-cvs-startup.el,v 1.1.1.3 1999/09/02 04:55:19 wsanchez Exp $"
;;;
(autoload 'cvs-update "pcl-cvs"
	  "Run a 'cvs update' in the current working directory. Feed the
output to a *cvs* buffer and run cvs-mode on it.
If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
	  t)

(autoload 'cvs-update-other-window "pcl-cvs"
	  "Run a 'cvs update' in the current working directory. Feed the
output to a *cvs* buffer, display it in the other window, and run
cvs-mode on it.

If optional prefix argument LOCAL is non-nil, 'cvs update -l' is run."
	  t)
