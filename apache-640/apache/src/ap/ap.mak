# Microsoft Developer Studio Generated NMAKE File, Based on ap.dsp
!IF "$(CFG)" == ""
CFG=ap - Win32 Debug
!MESSAGE No configuration specified. Defaulting to ap - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "ap - Win32 Release" && "$(CFG)" != "ap - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ap.mak" CFG="ap - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ap - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "ap - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "ap - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\ap.lib"

!ELSE 

ALL : "$(OUTDIR)\ap.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\ap_cpystrn.obj"
	-@erase "$(INTDIR)\ap_fnmatch.obj"
	-@erase "$(INTDIR)\ap_md5c.obj"
	-@erase "$(INTDIR)\ap_signal.obj"
	-@erase "$(INTDIR)\ap_slack.obj"
	-@erase "$(INTDIR)\ap_snprintf.obj"
	-@erase "$(INTDIR)\ap_sha1.obj"
	-@erase "$(INTDIR)\ap_base64.obj"
	-@erase "$(INTDIR)\ap_checkpass.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\ap.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "../include" /D "WIN32" /D "NDEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\ap.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\ap.bsc" 
BSC32_SBRS= \

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\ap.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ap_cpystrn.obj" \
	"$(INTDIR)\ap_fnmatch.obj" \
	"$(INTDIR)\ap_md5c.obj" \
	"$(INTDIR)\ap_signal.obj" \
	"$(INTDIR)\ap_slack.obj" \
	"$(INTDIR)\ap_snprintf.obj" \
	"$(INTDIR)\ap_sha1.obj" \
	"$(INTDIR)\ap_base64.obj" \
	"$(INTDIR)\ap_checkpass.obj"

"$(OUTDIR)\ap.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\ap.lib"

!ELSE 

ALL : "$(OUTDIR)\ap.lib"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\ap_cpystrn.obj"
	-@erase "$(INTDIR)\ap_fnmatch.obj"
	-@erase "$(INTDIR)\ap_md5c.obj"
	-@erase "$(INTDIR)\ap_signal.obj"
	-@erase "$(INTDIR)\ap_slack.obj"
	-@erase "$(INTDIR)\ap_snprintf.obj"
	-@erase "$(INTDIR)\ap_sha1.obj"
	-@erase "$(INTDIR)\ap_base64.obj"
	-@erase "$(INTDIR)\ap_checkpass.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(OUTDIR)\ap.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /I "../include" /D "WIN32" /D "_DEBUG" /D\
 "_WINDOWS" /Fp"$(INTDIR)\ap.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\ap.bsc" 
BSC32_SBRS= \

LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\ap.lib" 
LIB32_OBJS= \
	"$(INTDIR)\ap_cpystrn.obj" \
	"$(INTDIR)\ap_fnmatch.obj" \
	"$(INTDIR)\ap_md5c.obj" \
	"$(INTDIR)\ap_signal.obj" \
	"$(INTDIR)\ap_slack.obj" \
	"$(INTDIR)\ap_snprintf.obj" \
	"$(INTDIR)\ap_sha1.obj" \
	"$(INTDIR)\ap_base64.obj" \
	"$(INTDIR)\ap_checkpass.obj"

"$(OUTDIR)\ap.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "ap - Win32 Release" || "$(CFG)" == "ap - Win32 Debug"
SOURCE=.\ap_cpystrn.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_CP=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_CP=\
	"..\include\ap_config_auto.h"\
	"..\include\ebcdic.h"\
	"..\include\os.h"\
	"..\include\sfio.h"\


"$(INTDIR)\ap_cpystrn.obj" : $(SOURCE) $(DEP_CPP_AP_CP) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_CP=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\


"$(INTDIR)\ap_cpystrn.obj" : $(SOURCE) $(DEP_CPP_AP_CP) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_fnmatch.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_FN=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\fnmatch.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_FN=\
	"..\include\ap_config_auto.h"\
	"..\include\os.h"\


"$(INTDIR)\ap_fnmatch.obj" : $(SOURCE) $(DEP_CPP_AP_FN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_FN=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\fnmatch.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\


"$(INTDIR)\ap_fnmatch.obj" : $(SOURCE) $(DEP_CPP_AP_FN) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_signal.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_SI=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_SI=\
	"..\include\ap_config_auto.h"\
	"..\include\ebcdic.h"\
	"..\include\os.h"\
	"..\include\sfio.h"\


"$(INTDIR)\ap_signal.obj" : $(SOURCE) $(DEP_CPP_AP_SI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_SI=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\


"$(INTDIR)\ap_signal.obj" : $(SOURCE) $(DEP_CPP_AP_SI) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_slack.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_SL=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\http_log.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_SL=\
	"..\include\ap_config_auto.h"\
	"..\include\ebcdic.h"\
	"..\include\os.h"\
	"..\include\sfio.h"\


"$(INTDIR)\ap_slack.obj" : $(SOURCE) $(DEP_CPP_AP_SL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_SL=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\http_log.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\


"$(INTDIR)\ap_slack.obj" : $(SOURCE) $(DEP_CPP_AP_SL) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_snprintf.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_SN=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_SN=\
	"..\include\ap_config_auto.h"\
	"..\include\ebcdic.h"\
	"..\include\os.h"\
	"..\include\sfio.h"\


"$(INTDIR)\ap_snprintf.obj" : $(SOURCE) $(DEP_CPP_AP_SN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_SN=\
	"..\include\alloc.h"\
	"..\include\ap.h"\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\buff.h"\
	"..\include\hsregex.h"\
	"..\include\httpd.h"\
	"..\include\util_uri.h"\
	"..\os\win32\os.h"\
	"..\os\win32\readdir.h"\


"$(INTDIR)\ap_snprintf.obj" : $(SOURCE) $(DEP_CPP_AP_SN) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_md5c.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_MD=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_md5.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_MD=\
	"..\include\ap_config_auto.h"\
	"..\include\os.h"\
	".\ebcdic.h"\


"$(INTDIR)\ap_md5c.obj" : $(SOURCE) $(DEP_CPP_AP_MD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_MD=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_md5.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\


"$(INTDIR)\ap_md5c.obj" : $(SOURCE) $(DEP_CPP_AP_MD) "$(INTDIR)"


!ENDIF 


SOURCE=.\ap_checkpass.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_CH=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_CH=\
	"..\include\ap_config_auto.h"\
	"..\include\os.h"\
	".\ebcdic.h"\


"$(INTDIR)\ap_checkpass.obj" : $(SOURCE) $(DEP_CPP_AP_CH) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_CH=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\


"$(INTDIR)\ap_checkpass.obj" : $(SOURCE) $(DEP_CPP_AP_CH) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_sha1.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_SH=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_sha1.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_SH=\
	"..\include\ap_config_auto.h"\
	"..\include\os.h"\
	".\ebcdic.h"\


"$(INTDIR)\ap_sha1.obj" : $(SOURCE) $(DEP_CPP_AP_SH) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_SH=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_sha1.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\


"$(INTDIR)\ap_sha1.obj" : $(SOURCE) $(DEP_CPP_AP_SH) "$(INTDIR)"


!ENDIF 

SOURCE=.\ap_base64.c

!IF  "$(CFG)" == "ap - Win32 Release"

DEP_CPP_AP_BA=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\
	{$(INCLUDE)}"sys\stat.h"\
	{$(INCLUDE)}"sys\types.h"\

NODEP_CPP_AP_BA=\
	"..\include\ap_config_auto.h"\
	"..\include\os.h"\
	".\ebcdic.h"\


"$(INTDIR)\ap_base64.obj" : $(SOURCE) $(DEP_CPP_AP_BA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "ap - Win32 Debug"

DEP_CPP_AP_BA=\
	"..\include\ap_config.h"\
	"..\include\ap_ctype.h"\
	"..\include\ap_mmn.h"\
	"..\include\hsregex.h"\
	"..\os\win32\os.h"\


"$(INTDIR)\ap_base64.obj" : $(SOURCE) $(DEP_CPP_AP_BA) "$(INTDIR)"


!ENDIF 

!ENDIF 

