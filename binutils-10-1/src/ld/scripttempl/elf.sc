#
# Unusual variables checked by this code:
#	NOP - two byte opcode for no-op (defaults to 0)
#	DATA_ADDR - if end-of-text-plus-one-page isn't right for data start
#	INITIAL_READONLY_SECTIONS - at start of text segment
#	OTHER_READONLY_SECTIONS - other than .text .init .rodata ...
#		(e.g., .PARISC.milli)
#	OTHER_TEXT_SECTIONS - these get put in .text when relocating
#	OTHER_READWRITE_SECTIONS - other than .data .bss .ctors .sdata ...
#		(e.g., .PARISC.global)
#	OTHER_BSS_SECTIONS - other than .bss .sbss ...
#	OTHER_SECTIONS - at the end
#	EXECUTABLE_SYMBOLS - symbols that must be defined for an
#		executable (e.g., _DYNAMIC_LINK)
#	TEXT_START_SYMBOLS - symbols that appear at the start of the
#		.text section.
#	DATA_START_SYMBOLS - symbols that appear at the start of the
#		.data section.
#	OTHER_GOT_SYMBOLS - symbols defined just before .got.
#	OTHER_GOT_SECTIONS - sections just after .got and .sdata.
#	OTHER_BSS_SYMBOLS - symbols that appear at the start of the
#		.bss section besides __bss_start.
#	DATA_PLT - .plt should be in data segment, not text segment.
#	TEXT_DYNAMIC - .dynamic in text segment, not data segment.
#	EMBEDDED - whether this is for an embedded system. 
#	SHLIB_TEXT_START_ADDR - if set, add to SIZEOF_HEADERS to set
#		start address of shared library.
#	INPUT_FILES - INPUT command of files to always include
#	WRITABLE_RODATA - if set, the .rodata section should be writable
#	INIT_START, INIT_END -  statements just before and just after
# 	combination of .init sections.
#	FINI_START, FINI_END - statements just before and just after
# 	combination of .fini sections.
#
# When adding sections, do note that the names of some sections are used
# when specifying the start address of the next.
#

test -z "$ENTRY" && ENTRY=_start
test -z "${BIG_OUTPUT_FORMAT}" && BIG_OUTPUT_FORMAT=${OUTPUT_FORMAT}
test -z "${LITTLE_OUTPUT_FORMAT}" && LITTLE_OUTPUT_FORMAT=${OUTPUT_FORMAT}
if [ -z "$MACHINE" ]; then OUTPUT_ARCH=${ARCH}; else OUTPUT_ARCH=${ARCH}:${MACHINE}; fi
test -z "${ELFSIZE}" && ELFSIZE=32
test -z "${ALIGNMENT}" && ALIGNMENT="${ELFSIZE} / 8"
test "$LD_FLAG" = "N" && DATA_ADDR=.
INTERP=".interp   ${RELOCATING-0} : { *(.interp) 	}"
PLT=".plt    ${RELOCATING-0} : { *(.plt)	}"
DYNAMIC=".dynamic     ${RELOCATING-0} : { *(.dynamic) }"
RODATA=".rodata ${RELOCATING-0} : { *(.rodata) ${RELOCATING+*(.rodata.*)} ${RELOCATING+*(.gnu.linkonce.r*)} }"
CTOR=".ctors ${CONSTRUCTING-0} : 
  {
    ${CONSTRUCTING+${CTOR_START}}
    /* gcc uses crtbegin.o to find the start of
       the constructors, so we make sure it is
       first.  Because this is a wildcard, it
       doesn't matter if the user does not
       actually link against crtbegin.o; the
       linker won't look for a file to match a
       wildcard.  The wildcard also means that it
       doesn't matter which directory crtbegin.o
       is in.  */

    KEEP (*crtbegin.o(.ctors))

    /* We don't want to include the .ctor section from
       from the crtend.o file until after the sorted ctors.
       The .ctor section from the crtend file contains the
       end of ctors marker and it must be last */

    KEEP (*(EXCLUDE_FILE (*crtend.o $OTHER_EXCLUDE_FILES) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    ${CONSTRUCTING+${CTOR_END}}
  }"

DTOR=" .dtors       ${CONSTRUCTING-0} :
  {
    ${CONSTRUCTING+${DTOR_START}}
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o $OTHER_EXCLUDE_FILES) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    ${CONSTRUCTING+${DTOR_END}}
  }"

# if this is for an embedded system, don't add SIZEOF_HEADERS.
if [ -z "$EMBEDDED" ]; then
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR} + SIZEOF_HEADERS"
else
   test -z "${TEXT_BASE_ADDRESS}" && TEXT_BASE_ADDRESS="${TEXT_START_ADDR}"
fi

cat <<EOF
OUTPUT_FORMAT("${OUTPUT_FORMAT}", "${BIG_OUTPUT_FORMAT}",
	      "${LITTLE_OUTPUT_FORMAT}")
OUTPUT_ARCH(${OUTPUT_ARCH})
ENTRY(${ENTRY})

${RELOCATING+${LIB_SEARCH_DIRS}}
${RELOCATING+/* Do we need any of these for elf?
   __DYNAMIC = 0; ${STACKZERO+${STACKZERO}} ${SHLIB_PATH+${SHLIB_PATH}}  */}
${RELOCATING+${EXECUTABLE_SYMBOLS}}
${RELOCATING+${INPUT_FILES}}
${RELOCATING- /* For some reason, the Solaris linker makes bad executables
  if gld -r is used and the intermediate file has sections starting
  at non-zero addresses.  Could be a Solaris ld bug, could be a GNU ld
  bug.  But for now assigning the zero vmas works.  */}

SECTIONS
{
  /* Read-only sections, merged into text segment: */
  ${CREATE_SHLIB-${RELOCATING+. = ${TEXT_BASE_ADDRESS};}}
  ${CREATE_SHLIB+${RELOCATING+. = ${SHLIB_TEXT_START_ADDR:-0} + SIZEOF_HEADERS;}}
  ${CREATE_SHLIB-${INTERP}}
  ${INITIAL_READONLY_SECTIONS}
  ${TEXT_DYNAMIC+${DYNAMIC}}
  .hash        ${RELOCATING-0} : { *(.hash)		}
  .dynsym      ${RELOCATING-0} : { *(.dynsym)		}
  .dynstr      ${RELOCATING-0} : { *(.dynstr)		}
  .gnu.version ${RELOCATING-0} : { *(.gnu.version)	}
  .gnu.version_d ${RELOCATING-0} : { *(.gnu.version_d)	}
  .gnu.version_r ${RELOCATING-0} : { *(.gnu.version_r)	}

  .rel.init    ${RELOCATING-0} : { *(.rel.init)	}
  .rela.init   ${RELOCATING-0} : { *(.rela.init)	}
  .rel.text    ${RELOCATING-0} :
    {
      *(.rel.text)
      ${RELOCATING+*(.rel.text.*)}
      ${RELOCATING+*(.rel.gnu.linkonce.t*)}
    }
  .rela.text   ${RELOCATING-0} :
    {
      *(.rela.text)
      ${RELOCATING+*(.rela.text.*)}
      ${RELOCATING+*(.rela.gnu.linkonce.t*)}
    }
  .rel.fini    ${RELOCATING-0} : { *(.rel.fini)	}
  .rela.fini   ${RELOCATING-0} : { *(.rela.fini)	}
  .rel.rodata  ${RELOCATING-0} :
    {
      *(.rel.rodata)
      ${RELOCATING+*(.rel.rodata.*)}
      ${RELOCATING+*(.rel.gnu.linkonce.r*)}
    }
  .rela.rodata ${RELOCATING-0} :
    {
      *(.rela.rodata)
      ${RELOCATING+*(.rela.rodata.*)}
      ${RELOCATING+*(.rela.gnu.linkonce.r*)}
    }
  ${OTHER_READONLY_RELOC_SECTIONS}
  .rel.data    ${RELOCATING-0} :
    {
      *(.rel.data)
      ${RELOCATING+*(.rel.data.*)}
      ${RELOCATING+*(.rel.gnu.linkonce.d*)}
    }
  .rela.data   ${RELOCATING-0} :
    {
      *(.rela.data)
      ${RELOCATING+*(.rela.data.*)}
      ${RELOCATING+*(.rela.gnu.linkonce.d*)}
    }
  .rel.ctors   ${RELOCATING-0} : { *(.rel.ctors)	}
  .rela.ctors  ${RELOCATING-0} : { *(.rela.ctors)	}
  .rel.dtors   ${RELOCATING-0} : { *(.rel.dtors)	}
  .rela.dtors  ${RELOCATING-0} : { *(.rela.dtors)	}
  .rel.got     ${RELOCATING-0} : { *(.rel.got)		}
  .rela.got    ${RELOCATING-0} : { *(.rela.got)		}
  ${OTHER_GOT_RELOC_SECTIONS}
  .rel.sdata   ${RELOCATING-0} :
    {
      *(.rel.sdata)
      ${RELOCATING+*(.rel.sdata.*)}
      ${RELOCATING+*(.rel.gnu.linkonce.s*)}
    }
  .rela.sdata   ${RELOCATING-0} :
    {
      *(.rela.sdata)
      ${RELOCATING+*(.rela.sdata.*)}
      ${RELOCATING+*(.rela.gnu.linkonce.s*)}
    }
  .rel.sbss    ${RELOCATING-0} : { *(.rel.sbss)		}
  .rela.sbss   ${RELOCATING-0} : { *(.rela.sbss)	}
  .rel.bss     ${RELOCATING-0} : { *(.rel.bss)		}
  .rela.bss    ${RELOCATING-0} : { *(.rela.bss)		}
  .rel.plt     ${RELOCATING-0} : { *(.rel.plt)		}
  .rela.plt    ${RELOCATING-0} : { *(.rela.plt)		}
  ${OTHER_PLT_RELOC_SECTIONS}

  .init        ${RELOCATING-0} : 
  { 
    ${INIT_START}
    KEEP (*(.init))
    ${INIT_END}
  } =${NOP-0}

  ${DATA_PLT-${PLT}}
  .text    ${RELOCATING-0} :
  {
    ${RELOCATING+${TEXT_START_SYMBOLS}}
    *(.text)
    ${RELOCATING+*(.text.*)}
    *(.stub)
    /* .gnu.warning sections are handled specially by elf32.em.  */
    *(.gnu.warning)
    ${RELOCATING+*(.gnu.linkonce.t*)}
    ${RELOCATING+${OTHER_TEXT_SECTIONS}}
  } =${NOP-0}
  ${RELOCATING+_etext = .;}
  ${RELOCATING+PROVIDE (etext = .);}
  .fini    ${RELOCATING-0} :
  {
    ${FINI_START}
    KEEP (*(.fini))
    ${FINI_END}
  } =${NOP-0}
  ${WRITABLE_RODATA-${RODATA}}
  .rodata1 ${RELOCATING-0} : { *(.rodata1) }
  ${RELOCATING+${OTHER_READONLY_SECTIONS}}

  /* Adjust the address for the data segment.  We want to adjust up to
     the same address within the page on the next page up.  */
  ${CREATE_SHLIB-${RELOCATING+. = ${DATA_ADDR-ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1))};}}
  ${CREATE_SHLIB+${RELOCATING+. = ${SHLIB_DATA_ADDR-ALIGN(${MAXPAGESIZE}) + (. & (${MAXPAGESIZE} - 1))};}}

  .data  ${RELOCATING-0} :
  {
    ${RELOCATING+${DATA_START_SYMBOLS}}
    *(.data)
    ${RELOCATING+*(.data.*)}
    ${RELOCATING+*(.gnu.linkonce.d*)}
    ${CONSTRUCTING+SORT(CONSTRUCTORS)}
  }
  .data1 ${RELOCATING-0} : { *(.data1) }
  .eh_frame : { *(.eh_frame) }
  .gcc_except_table : { *(.gcc_except_table) }
  ${WRITABLE_RODATA+${RODATA}}
  ${RELOCATING+${OTHER_READWRITE_SECTIONS}}
  ${RELOCATING+${CTOR}}
  ${RELOCATING+${DTOR}}
  ${DATA_PLT+${PLT}}
  ${RELOCATING+${OTHER_GOT_SYMBOLS}}
  .got         ${RELOCATING-0} : { *(.got.plt) *(.got) }
  ${TEXT_DYNAMIC-${DYNAMIC}}
  /* We want the small data sections together, so single-instruction offsets
     can access them all, and initialized data all before uninitialized, so
     we can shorten the on-disk segment size.  */
  .sdata   ${RELOCATING-0} : 
  {
    ${RELOCATING+${SDATA_START_SYMBOLS}}
    *(.sdata) 
    ${RELOCATING+*(.sdata.*)}
    ${RELOCATING+*(.gnu.linkonce.s.*)}
  }
  ${RELOCATING+${OTHER_GOT_SECTIONS}}
  ${RELOCATING+_edata = .;}
  ${RELOCATING+PROVIDE (edata = .);}
  ${RELOCATING+__bss_start = .;}
  ${RELOCATING+${OTHER_BSS_SYMBOLS}}
  .sbss    ${RELOCATING-0} :
  {
    *(.dynsbss)
    *(.sbss)
    ${RELOCATING+*(.sbss.*)}
    *(.scommon)
  }
  .bss     ${RELOCATING-0} :
  {
   *(.dynbss)
   *(.bss)
   ${RELOCATING+*(.bss.*)}
   *(COMMON)
   /* Align here to ensure that the .bss section occupies space up to
      _end.  Align after .bss to ensure correct alignment even if the
      .bss section disappears because there are no input sections.  */
   ${RELOCATING+. = ALIGN(${ALIGNMENT});}
  }
  ${RELOCATING+${OTHER_BSS_SECTIONS}}
  ${RELOCATING+. = ALIGN(${ALIGNMENT});}
  ${RELOCATING+_end = .;}
  ${RELOCATING+${OTHER_BSS_END_SYMBOLS}}
  ${RELOCATING+PROVIDE (end = .);}

  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  .stab.excl 0 : { *(.stab.excl) }
  .stab.exclstr 0 : { *(.stab.exclstr) }
  .stab.index 0 : { *(.stab.index) }
  .stab.indexstr 0 : { *(.stab.indexstr) }

  .comment 0 : { *(.comment) }

  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */

  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }

  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }

  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }

  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }

  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }

  ${RELOCATING+${OTHER_RELOCATING_SECTIONS}}

  /* These must appear regardless of ${RELOCATING}.  */
  ${OTHER_SECTIONS}
}
EOF
