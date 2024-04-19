# package.tcl --
#
# utility procs formerly in init.tcl which can be loaded on demand
# for package management.
#
# RCS: @(#) $Id: package.tcl,v 1.1.1.1 1999/11/09 21:55:20 wsanchez Exp $
#
# Copyright (c) 1991-1993 The Regents of the University of California.
# Copyright (c) 1994-1998 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# pkg_compareExtension --
#
#  Used internally by pkg_mkIndex to compare the extension of a file to
#  a given extension. On Windows, it uses a case-insensitive comparison
#  because the file system can be file insensitive.
#
# Arguments:
#  fileName	name of a file whose extension is compared
#  ext		(optional) The extension to compare against; you must
#		provide the starting dot.
#		Defaults to [info sharedlibextension]
#
# Results:
#  Returns 1 if the extension matches, 0 otherwise

proc pkg_compareExtension { fileName {ext {}} } {
    global tcl_platform
    if {[string length $ext] == 0} {
	set ext [info sharedlibextension]
    }
    if {[string compare $tcl_platform(platform) "windows"] == 0} {
	return [expr {[string compare \
		[string tolower [file extension $fileName]] \
		[string tolower $ext]] == 0}]
    } else {
	return [expr {[string compare [file extension $fileName] $ext] == 0}]
    }
}

# pkg_mkIndex --
# This procedure creates a package index in a given directory.  The
# package index consists of a "pkgIndex.tcl" file whose contents are
# a Tcl script that sets up package information with "package require"
# commands.  The commands describe all of the packages defined by the
# files given as arguments.
#
# Arguments:
# -direct		(optional) If this flag is present, the generated
#			code in pkgMkIndex.tcl will cause the package to be
#			loaded when "package require" is executed, rather
#			than lazily when the first reference to an exported
#			procedure in the package is made.
# -verbose		(optional) Verbose output; the name of each file that
#			was successfully rocessed is printed out. Additionally,
#			if processing of a file failed a message is printed.
# -load pat		(optional) Preload any packages whose names match
#			the pattern.  Used to handle DLLs that depend on
#			other packages during their Init procedure.
# dir -			Name of the directory in which to create the index.
# args -		Any number of additional arguments, each giving
#			a glob pattern that matches the names of one or
#			more shared libraries or Tcl script files in
#			dir.

proc pkg_mkIndex {args} {
    global errorCode errorInfo
    set usage {"pkg_mkIndex ?-direct? ?-verbose? ?-load pattern? ?--? dir ?pattern ...?"};

    set argCount [llength $args]
    if {$argCount < 1} {
	return -code error "wrong # args: should be\n$usage"
    }

    set more ""
    set direct 0
    set doVerbose 0
    set loadPat ""
    for {set idx 0} {$idx < $argCount} {incr idx} {
	set flag [lindex $args $idx]
	switch -glob -- $flag {
	    -- {
		# done with the flags
		incr idx
		break
	    }
	    -verbose {
		set doVerbose 1
	    }
	    -direct {
		set direct 1
		append more " -direct"
	    }
	    -load {
		incr idx
		set loadPat [lindex $args $idx]
		append more " -load $loadPat"
	    }
	    -* {
		return -code error "unknown flag $flag: should be\n$usage"
	    }
	    default {
		# done with the flags
		break
	    }
	}
    }

    set dir [lindex $args $idx]
    set patternList [lrange $args [expr {$idx + 1}] end]
    if {[llength $patternList] == 0} {
	set patternList [list "*.tcl" "*[info sharedlibextension]"]
    }

    append index "# Tcl package index file, version 1.1\n"
    append index "# This file is generated by the \"pkg_mkIndex$more\" command\n"
    append index "# and sourced either when an application starts up or\n"
    append index "# by a \"package unknown\" script.  It invokes the\n"
    append index "# \"package ifneeded\" command to set up package-related\n"
    append index "# information so that packages will be loaded automatically\n"
    append index "# in response to \"package require\" commands.  When this\n"
    append index "# script is sourced, the variable \$dir must contain the\n"
    append index "# full path name of this file's directory.\n"
    set oldDir [pwd]
    cd $dir

    if {[catch {eval glob $patternList} fileList]} {
	global errorCode errorInfo
	cd $oldDir
	return -code error -errorcode $errorCode -errorinfo $errorInfo $fileList
    }
    foreach file $fileList {
	# For each file, figure out what commands and packages it provides.
	# To do this, create a child interpreter, load the file into the
	# interpreter, and get a list of the new commands and packages
	# that are defined.

	if {[string compare $file "pkgIndex.tcl"] == 0} {
	    continue
	}

	# Changed back to the original directory before initializing the
	# slave in case TCL_LIBRARY is a relative path (e.g. in the test
	# suite). 

	cd $oldDir
	set c [interp create]

	# Load into the child any packages currently loaded in the parent
	# interpreter that match the -load pattern.

	foreach pkg [info loaded] {
	    if {! [string match $loadPat [lindex $pkg 1]]} {
		continue
	    }
	    if {[lindex $pkg 1] == "Tk"} {
		$c eval {set argv {-geometry +0+0}}
	    }
	    if {[catch {
		load [lindex $pkg 0] [lindex $pkg 1] $c
	    } err]} {
		if {$doVerbose} {
		    tclLog "warning: load [lindex $pkg 0] [lindex $pkg 1]\nfailed with: $err"
		}
	    } else {
		if {$doVerbose} {
		    tclLog "loaded [lindex $pkg 0] [lindex $pkg 1]"
		}
	    }
	}
	cd $dir

	$c eval {
	    # Stub out the package command so packages can
	    # require other packages.

	    rename package __package_orig
	    proc package {what args} {
		switch -- $what {
		    require { return ; # ignore transitive requires }
		    default { eval __package_orig {$what} $args }
		}
	    }
	    proc tclPkgUnknown args {}
	    package unknown tclPkgUnknown

	    # Stub out the unknown command so package can call
	    # into each other during their initialilzation.

	    proc unknown {args} {}

	    # Stub out the auto_import mechanism

	    proc auto_import {args} {}

	    # reserve the ::tcl namespace for support procs
	    # and temporary variables.  This might make it awkward
	    # to generate a pkgIndex.tcl file for the ::tcl namespace.

	    namespace eval ::tcl {
		variable file		;# Current file being processed
		variable direct		;# -direct flag value
		variable x		;# Loop variable
		variable debug		;# For debugging
		variable type		;# "load" or "source", for -direct
		variable namespaces	;# Existing namespaces (e.g., ::tcl)
		variable packages	;# Existing packages (e.g., Tcl)
		variable origCmds	;# Existing commands
		variable newCmds	;# Newly created commands
		variable newPkgs {}	;# Newly created packages
	    }
	}

	$c eval [list set ::tcl::file $file]
	$c eval [list set ::tcl::direct $direct]

	# Download needed procedures into the slave because we've
	# just deleted the unknown procedure.  This doesn't handle
	# procedures with default arguments.

	foreach p {pkg_compareExtension} {
	    $c eval [list proc $p [info args $p] [info body $p]]
	}

	if {[catch {
	    $c eval {
		set ::tcl::debug "loading or sourcing"

		# we need to track command defined by each package even in
		# the -direct case, because they are needed internally by
		# the "partial pkgIndex.tcl" step above.

		proc ::tcl::GetAllNamespaces {{root ::}} {
		    set list $root
		    foreach ns [namespace children $root] {
			eval lappend list [::tcl::GetAllNamespaces $ns]
		    }
		    return $list
		}

		# initialize the list of existing namespaces, packages, commands

		foreach ::tcl::x [::tcl::GetAllNamespaces] {
		    set ::tcl::namespaces($::tcl::x) 1
		}
		foreach ::tcl::x [package names] {
		    set ::tcl::packages($::tcl::x) 1
		}
		set ::tcl::origCmds [info commands]

		# Try to load the file if it has the shared library
		# extension, otherwise source it.  It's important not to
		# try to load files that aren't shared libraries, because
		# on some systems (like SunOS) the loader will abort the
		# whole application when it gets an error.

		if {[pkg_compareExtension $::tcl::file [info sharedlibextension]]} {
		    # The "file join ." command below is necessary.
		    # Without it, if the file name has no \'s and we're
		    # on UNIX, the load command will invoke the
		    # LD_LIBRARY_PATH search mechanism, which could cause
		    # the wrong file to be used.

		    set ::tcl::debug loading
		    load [file join . $::tcl::file]
		    set ::tcl::type load
		} else {
		    set ::tcl::debug sourcing
		    source $::tcl::file
		    set ::tcl::type source
		}

		# See what new namespaces appeared, and import commands
		# from them.  Only exported commands go into the index.

		foreach ::tcl::x [::tcl::GetAllNamespaces] {
		    if {! [info exists ::tcl::namespaces($::tcl::x)]} {
			namespace import -force ${::tcl::x}::*
		    }
		}

		# Figure out what commands appeared

		foreach ::tcl::x [info commands] {
		    set ::tcl::newCmds($::tcl::x) 1
		}
		foreach ::tcl::x $::tcl::origCmds {
		    catch {unset ::tcl::newCmds($::tcl::x)}
		}
		foreach ::tcl::x [array names ::tcl::newCmds] {
		    # reverse engineer which namespace a command comes from
		    
		    set ::tcl::abs [namespace origin $::tcl::x]

		    # special case so that global names have no leading
		    # ::, this is required by the unknown command

		    set ::tcl::abs [auto_qualify $::tcl::abs ::]

		    if {[string compare $::tcl::x $::tcl::abs] != 0} {
			# Name changed during qualification

			set ::tcl::newCmds($::tcl::abs) 1
			unset ::tcl::newCmds($::tcl::x)
		    }
		}

		# Look through the packages that appeared, and if there is
		# a version provided, then record it

		foreach ::tcl::x [package names] {
		    if {([string compare [package provide $::tcl::x] ""] != 0) \
			    && ![info exists ::tcl::packages($::tcl::x)]} {
			lappend ::tcl::newPkgs \
			    [list $::tcl::x [package provide $::tcl::x]]
		    }
		}
	    }
	} msg] == 1} {
	    set what [$c eval set ::tcl::debug]
	    if {$doVerbose} {
		tclLog "warning: error while $what $file: $msg"
	    }
	} else {
	    set type [$c eval set ::tcl::type]
	    set cmds [lsort [$c eval array names ::tcl::newCmds]]
	    set pkgs [$c eval set ::tcl::newPkgs]
	    if {[llength $pkgs] > 1} {
		tclLog "warning: \"$file\" provides more than one package ($pkgs)"
	    }
	    foreach pkg $pkgs {
		# cmds is empty/not used in the direct case
		lappend files($pkg) [list $file $type $cmds]
	    }

	    if {$doVerbose} {
		tclLog "processed $file"
	    }
	    interp delete $c
	}
    }

    foreach pkg [lsort [array names files]] {
	append index "\npackage ifneeded $pkg "
	if {$direct} {
	    set cmdList {}
	    foreach elem $files($pkg) {
		set file [lindex $elem 0]
		set type [lindex $elem 1]
		lappend cmdList "\[list $type \[file join \$dir\
			[list $file]\]\]"
	    }
	    append index [join $cmdList "\\n"]
	} else {
	    append index "\[list tclPkgSetup \$dir [lrange $pkg 0 0]\
		    [lrange $pkg 1 1] [list $files($pkg)]\]"
	}
    }
    set f [open pkgIndex.tcl w]
    puts $f $index
    close $f
    cd $oldDir
}

# tclPkgSetup --
# This is a utility procedure use by pkgIndex.tcl files.  It is invoked
# as part of a "package ifneeded" script.  It calls "package provide"
# to indicate that a package is available, then sets entries in the
# auto_index array so that the package's files will be auto-loaded when
# the commands are used.
#
# Arguments:
# dir -			Directory containing all the files for this package.
# pkg -			Name of the package (no version number).
# version -		Version number for the package, such as 2.1.3.
# files -		List of files that constitute the package.  Each
#			element is a sub-list with three elements.  The first
#			is the name of a file relative to $dir, the second is
#			"load" or "source", indicating whether the file is a
#			loadable binary or a script to source, and the third
#			is a list of commands defined by this file.

proc tclPkgSetup {dir pkg version files} {
    global auto_index

    package provide $pkg $version
    foreach fileInfo $files {
	set f [lindex $fileInfo 0]
	set type [lindex $fileInfo 1]
	foreach cmd [lindex $fileInfo 2] {
	    if {$type == "load"} {
		set auto_index($cmd) [list load [file join $dir $f] $pkg]
	    } else {
		set auto_index($cmd) [list source [file join $dir $f]]
	    } 
	}
    }
}

# tclMacPkgSearch --
# The procedure is used on the Macintosh to search a given directory for files
# with a TEXT resource named "pkgIndex".  If it exists it is sourced in to the
# interpreter to setup the package database.

proc tclMacPkgSearch {dir} {
    foreach x [glob -nocomplain [file join $dir *.shlb]] {
	if {[file isfile $x]} {
	    set res [resource open $x]
	    foreach y [resource list TEXT $res] {
		if {$y == "pkgIndex"} {source -rsrc pkgIndex}
	    }
	    catch {resource close $res}
	}
    }
}

# tclPkgUnknown --
# This procedure provides the default for the "package unknown" function.
# It is invoked when a package that's needed can't be found.  It scans
# the auto_path directories and their immediate children looking for
# pkgIndex.tcl files and sources any such files that are found to setup
# the package database.  (On the Macintosh we also search for pkgIndex
# TEXT resources in all files.)
#
# Arguments:
# name -		Name of desired package.  Not used.
# version -		Version of desired package.  Not used.
# exact -		Either "-exact" or omitted.  Not used.

proc tclPkgUnknown {name version {exact {}}} {
    global auto_path tcl_platform env

    if {![info exists auto_path]} {
	return
    }
    for {set i [expr {[llength $auto_path] - 1}]} {$i >= 0} {incr i -1} {
	# we can't use glob in safe interps, so enclose the following
	# in a catch statement
	catch {
	    foreach file [glob -nocomplain [file join [lindex $auto_path $i] \
		    * pkgIndex.tcl]] {
		set dir [file dirname $file]
		if {[file readable $file]} {
		    if {[catch {source $file} msg]} {
			tclLog "error reading package index file $file: $msg"
		    }
		}
	    }
	}
	set dir [lindex $auto_path $i]
	set file [file join $dir pkgIndex.tcl]
	# safe interps usually don't have "file readable", nor stderr channel
	if {[interp issafe] || [file readable $file]} {
	    if {[catch {source $file} msg] && ![interp issafe]}  {
		tclLog "error reading package index file $file: $msg"
	    }
	}
	# On the Macintosh we also look in the resource fork 
	# of shared libraries
	# We can't use tclMacPkgSearch in safe interps because it uses glob
	if {(![interp issafe]) && ($tcl_platform(platform) == "macintosh")} {
	    set dir [lindex $auto_path $i]
	    tclMacPkgSearch $dir
	    foreach x [glob -nocomplain [file join $dir *]] {
		if {[file isdirectory $x]} {
		    set dir $x
		    tclMacPkgSearch $dir
		}
	    }
	}
    }
}