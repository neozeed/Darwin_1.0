# Microsoft Developer Studio Generated NMAKE File, Based on lockview.dsp
!IF "$(CFG)" == ""
CFG=lockview - Win32 Release
!MESSAGE No configuration specified. Defaulting to lockview - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "lockview - Win32 Release" && "$(CFG)" != "lockview - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "lockview.mak" CFG="lockview - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "lockview - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "lockview - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "lockview - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\lockview.exe"


CLEAN :
	-@erase "$(INTDIR)\lockview.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\lockview.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /I "\apache-1.3\src\lib\expat-lite" /I "\apache-1.3\src\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\lockview.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\lockview.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../sdbm/Release/sdbm.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\lockview.pdb" /machine:I386 /out:"$(OUTDIR)\lockview.exe" 
LINK32_OBJS= \
	"$(INTDIR)\lockview.obj"

"$(OUTDIR)\lockview.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "lockview - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\lockview.exe"


CLEAN :
	-@erase "$(INTDIR)\lockview.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\lockview.exe"
	-@erase "$(OUTDIR)\lockview.ilk"
	-@erase "$(OUTDIR)\lockview.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /GX /Z7 /Od /I "\apache-1.3\src\lib\expat-lite" /I "\apache-1.3\src\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\lockview.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\lockview.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=../sdbm/Debug/sdbm.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\lockview.pdb" /debug /machine:I386 /out:"$(OUTDIR)\lockview.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\lockview.obj"

"$(OUTDIR)\lockview.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("lockview.dep")
!INCLUDE "lockview.dep"
!ELSE 
!MESSAGE Warning: cannot find "lockview.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "lockview - Win32 Release" || "$(CFG)" == "lockview - Win32 Debug"
SOURCE=.\lockview.c

!IF  "$(CFG)" == "lockview - Win32 Release"

CPP_SWITCHES=/nologo /MD /W3 /GX /O2 /I "\apache-1.3\src\lib\expat-lite" /I "\apache-1.3\src\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\lockview.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\lockview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "lockview - Win32 Debug"

CPP_SWITCHES=/nologo /MDd /W3 /GX /Z7 /Od /I "\apache-1.3\src\lib\expat-lite" /I "\apache-1.3\src\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\lockview.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\lockview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 


!ENDIF 

