/* Header for coding system handler.
   Copyright (C) 1995, 1997 Electrotechnical Laboratory, JAPAN.
   Licensed to the Free Software Foundation.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef _CODING_H
#define _CODING_H

#ifndef _CCL_H
#include "../src/ccl.h"
#endif

/*** EMACS' INTERNAL FORMAT (emacs-mule) section ***/

/* All code (1-byte) of Emacs' internal format is classified into one
   of the followings.  See also `charset.h'.  */
enum emacs_code_class_type
  {
    EMACS_control_code,		/* Control codes in the range
				   0x00..0x1F and 0x7F except for the
				   following two codes.  */
    EMACS_linefeed_code,	/* 0x0A (linefeed) to denote
				   end-of-line.  */
    EMACS_carriage_return_code,	/* 0x0D (carriage-return) to be used
				   in selective display mode.  */
    EMACS_ascii_code,		/* ASCII characters.  */
    EMACS_leading_code_composition, /* Leading code of a composite
				       character.  */
    EMACS_leading_code_2,	/* Base leading code of official
				   TYPE9N character.  */
    EMACS_leading_code_3,	/* Base leading code of private TYPE9N
				   or official TYPE9Nx9N character.  */
    EMACS_leading_code_4,	/* Base leading code of private
				   TYPE9Nx9N character.  */
    EMACS_invalid_code		/* Invalid code, i.e. a base leading
				   code not yet assigned to any
				   charset, or a code of the range
				   0xA0..0xFF.  */
  };

extern enum emacs_code_class_type emacs_code_class[256];

/*** ISO2022 section ***/

/* Macros to define code of control characters for ISO2022's functions.  */
			/* code */	/* function */
#define ISO_CODE_LF	0x0A		/* line-feed */
#define ISO_CODE_CR	0x0D		/* carriage-return */
#define ISO_CODE_SO	0x0E		/* shift-out */
#define ISO_CODE_SI	0x0F		/* shift-in */
#define ISO_CODE_SS2_7	0x19		/* single-shift-2 for 7-bit code */
#define ISO_CODE_ESC	0x1B		/* escape */
#define ISO_CODE_SS2	0x8E		/* single-shift-2 */
#define ISO_CODE_SS3	0x8F		/* single-shift-3 */
#define ISO_CODE_CSI	0x9B		/* control-sequence-introduce */

/* All code (1-byte) of ISO2022 is classified into one of the
   followings.  */
enum iso_code_class_type
  {
    ISO_control_code,		/* Control codes in the range
				   0x00..0x1F, 0x7F, and 0x80..0x9F,
				   except for the following seven
				   codes.  */
    ISO_carriage_return,	/* ISO_CODE_CR (0x0D) */
    ISO_shift_out,		/* ISO_CODE_SO (0x0E) */
    ISO_shift_in,		/* ISO_CODE_SI (0x0F) */
    ISO_single_shift_2_7,	/* ISO_CODE_SS2_7 (0x19) */
    ISO_escape,			/* ISO_CODE_SO (0x1B) */
    ISO_single_shift_2,		/* ISO_CODE_SS2 (0x8E) */
    ISO_single_shift_3,		/* ISO_CODE_SS3 (0x8F) */
    ISO_control_sequence_introducer, /* ISO_CODE_CSI (0x9B) */
    ISO_0x20_or_0x7F,		/* Codes of the values 0x20 or 0x7F.  */
    ISO_graphic_plane_0,	/* Graphic codes in the range 0x21..0x7E.  */
    ISO_0xA0_or_0xFF,		/* Codes of the values 0xA0 or 0xFF.  */
    ISO_graphic_plane_1		/* Graphic codes in the range 0xA1..0xFE.  */
  };

/** The macros CODING_FLAG_ISO_XXX defines a flag bit of the `flags'
  element in the structure `coding_system'.  This information is used
  while encoding a text to ISO2022.  **/

/* If set, produce short-form designation sequence (e.g. ESC $ A)
   instead of long-form sequence (e.g. ESC $ ( A).  */
#define CODING_FLAG_ISO_SHORT_FORM	0x0001

/* If set, reset graphic planes and registers at end-of-line to the
   initial state.  */
#define CODING_FLAG_ISO_RESET_AT_EOL	0x0002

/* If set, reset graphic planes and registers before any control
   characters to the initial state.  */
#define CODING_FLAG_ISO_RESET_AT_CNTL	0x0004

/* If set, encode by 7-bit environment.  */
#define CODING_FLAG_ISO_SEVEN_BITS	0x0008

/* If set, use locking-shift function.  */
#define CODING_FLAG_ISO_LOCKING_SHIFT	0x0010

/* If set, use single-shift function.  Overwrite
   CODING_FLAG_ISO_LOCKING_SHIFT.  */
#define CODING_FLAG_ISO_SINGLE_SHIFT	0x0020

/* If set, designate JISX0201-Roman instead of ASCII.  */
#define CODING_FLAG_ISO_USE_ROMAN	0x0040

/* If set, designate JISX0208-1978 instead of JISX0208-1983.  */
#define CODING_FLAG_ISO_USE_OLDJIS	0x0080

/* If set, do not produce ISO6429's direction specifying sequence.  */
#define CODING_FLAG_ISO_NO_DIRECTION	0x0100

/* If set, assume designation states are reset at beginning of line on
   output.  */
#define CODING_FLAG_ISO_INIT_AT_BOL	0x0200

/* If set, designation sequence should be placed at beginning of line
   on output.  */
#define CODING_FLAG_ISO_DESIGNATE_AT_BOL 0x0400

/* If set, do not encode unsafe charactes on output.  */
#define CODING_FLAG_ISO_SAFE		0x0800

/* If set, extra latin codes (128..159) are accepted as a valid code
   on input.  */
#define CODING_FLAG_ISO_LATIN_EXTRA	0x1000

/* If set, use designation escape sequence.  */
#define CODING_FLAG_ISO_DESIGNATION	0x10000

/* A character to be produced on output if encoding of the original
   character is prohibited by CODING_FLAG_ISO_SAFE.  */
#define CODING_INHIBIT_CHARACTER_SUBSTITUTION  077 /* 077 == `?' */

/* Structure of the field `spec.iso2022' in the structure `coding_system'.  */
struct iso2022_spec
{
  /* The current graphic register invoked to each graphic plane.  */
  int current_invocation[2];

  /* The current charset designated to each graphic register.  */
  int current_designation[4];

  /* A charset initially designated to each graphic register.  */
  int initial_designation[4];

  /* If not -1, it is a graphic register specified in an invalid
     designation sequence.  */
  int last_invalid_designation_register;

  /* A graphic register to which each charset should be designated.  */
  unsigned char requested_designation[MAX_CHARSET + 1];

  /* A revision number to be specified for each charset on encoding.
     The value 255 means no revision number for the corresponding
     charset.  */
  unsigned char charset_revision_number[MAX_CHARSET + 1];

  /* Set to 1 temporarily only when graphic register 2 or 3 is invoked
     by single-shift while encoding.  */
  int single_shifting;

  /* Set to 1 temporarily only when processing at beginning of line.  */
  int bol;
};

/* Macros to access each field in the structure `spec.iso2022'.  */
#define CODING_SPEC_ISO_INVOCATION(coding, plane) \
  (coding)->spec.iso2022.current_invocation[plane]
#define CODING_SPEC_ISO_DESIGNATION(coding, reg) \
  (coding)->spec.iso2022.current_designation[reg]
#define CODING_SPEC_ISO_INITIAL_DESIGNATION(coding, reg) \
  (coding)->spec.iso2022.initial_designation[reg]
#define CODING_SPEC_ISO_REQUESTED_DESIGNATION(coding, charset) \
  (coding)->spec.iso2022.requested_designation[charset]
#define CODING_SPEC_ISO_REVISION_NUMBER(coding, charset) \
  (coding)->spec.iso2022.charset_revision_number[charset]
#define CODING_SPEC_ISO_SINGLE_SHIFTING(coding) \
  (coding)->spec.iso2022.single_shifting
#define CODING_SPEC_ISO_BOL(coding) \
  (coding)->spec.iso2022.bol

/* A value which may appear in
   coding->spec.iso2022.requested_designation indicating that the
   corresponding charset does not request any graphic register to be
   designated.  */
#define CODING_SPEC_ISO_NO_REQUESTED_DESIGNATION 4

/* Return a charset which is currently designated to the graphic plane
   PLANE in the coding-system CODING.  */
#define CODING_SPEC_ISO_PLANE_CHARSET(coding, plane)	\
  ((CODING_SPEC_ISO_INVOCATION (coding, plane) < 0)	\
   ? -1							\
   : CODING_SPEC_ISO_DESIGNATION (coding,		\
				  CODING_SPEC_ISO_INVOCATION (coding, plane)))

/*** BIG5 section ***/

/* Macros to denote each type of BIG5 coding system.  */
#define CODING_FLAG_BIG5_HKU	0x00 /* BIG5-HKU is one of variants of
					BIG5 developed by Hong Kong
					University.  */
#define CODING_FLAG_BIG5_ETEN	0x01 /* BIG5_ETen is one of variants
					of BIG5 developed by the
					company ETen in Taiwan.  */

/*** GENERAL section ***/

/* Types of coding system.  */
enum coding_type
  {
    coding_type_no_conversion,	/* A coding system which requires no
				   conversion for reading and writing
				   including end-of-line format.  */
    coding_type_emacs_mule,	/* A coding system used in Emacs'
				   buffer and string.  Requires no
				   conversion for reading and writing
				   except for end-of-line format.  */
    coding_type_undecided,	/* A coding system which requires
				   automatic detection of a real
				   coding system.  */
    coding_type_sjis,		/* SJIS coding system for Japanese.  */
    coding_type_iso2022,	/* Any coding system of ISO2022
				   variants.  */
    coding_type_big5,		/* BIG5 coding system for Chinese.  */
    coding_type_ccl,		/* The coding system of which decoder
				   and encoder are written in CCL.  */
    coding_type_raw_text	/* A coding system for a text
				   containing ramdom 8-bit code which
				   does not require code conversion
				   except for end-of-line format. */
  };

/* Formats of end-of-line.  */
#define CODING_EOL_LF	0	/* Line-feed only, same as Emacs'
				   internal format.  */
#define CODING_EOL_CRLF	1	/* Sequence of carriage-return and
				   line-feed.  */
#define CODING_EOL_CR	2	/* Carriage-return only.  */
#define CODING_EOL_UNDECIDED 3	/* This value is used to denote the
				   eol-type is not yet decided.  */
#define CODING_EOL_INCONSISTENT 4 /* This value is used to denote the
				     eol-type is not consistent
				     through the file.  */

/* Character composition status while encoding/decoding.  */
#define COMPOSING_NO		 0 /* not composing */
#define COMPOSING_WITH_RULE_HEAD 1 /* 1st char of with-rule composing follow */
#define COMPOSING_NO_RULE_HEAD	 2 /* 1st char of no-rule composing follow */
#define COMPOSING_WITH_RULE_TAIL 3 /* Nth char of with-rule composing follow */
#define COMPOSING_NO_RULE_TAIL	 4 /* Nth char of no-rule composing follow */
#define COMPOSING_WITH_RULE_RULE 5 /* composition rule follow */

/* 1 iff composing.  */
#define COMPOSING_P(composing) (composing)
/* 1 iff 1st char of composing element follows.  */
#define COMPOSING_HEAD_P(composing) \
  ((composing) && (composing) <= COMPOSING_NO_RULE_HEAD)
/* 1 iff composing with embeded composition rule.  */
#define COMPOSING_WITH_RULE_P(composing) ((composing) & 1)

/* Macros used for the member finish_status of the struct
   coding_system.  */
#define CODING_FINISH_NORMAL		0
#define CODING_FINISH_INSUFFICIENT_SRC	1
#define CODING_FINISH_INSUFFICIENT_DST	2
#define CODING_FINISH_INCONSISTENT_EOL	3

/* Macros used for the member `mode' of the struct coding_system.  */

/* If set, recover the original CR or LF of the already decoded text
   when the decoding routine encounters an inconsistent eol format.  */
#define CODING_MODE_INHIBIT_INCONSISTENT_EOL	0x01

/* If set, the decoding/encoding routines treat the current data as
   the last block of the whole text to be converted, and do
   appropriate fisishing job.  */
#define CODING_MODE_LAST_BLOCK			0x02

/* If set, it means that the current source text is in a buffer which
   enables selective display.  */
#define CODING_MODE_SELECTIVE_DISPLAY		0x04

/* This flag is used by the decoding/encoding routines on the fly.  If
   set, it means that right-to-left text is being processed.  */
#define CODING_MODE_DIRECTION			0x08

struct coding_system
{
  /* Type of the coding system.  */
  enum coding_type type;

  /* Type of end-of-line format (LF, CRLF, or CR) of the coding system.  */
  int eol_type;

  /* Flag bits of the coding system.  The meaning of each bit is common
     to all types of coding systems.  */
  unsigned int common_flags;

  /* Flag bits of the coding system.  The meaning of each bit depends
     on the type of the coding system.  */
  unsigned int flags;

  /* Mode bits of the coding system.  See the comments of the macros
     CODING_MODE_XXX.  */
  unsigned int mode;

  /* Table of safe character sets for this coding system.  If the Nth
     element is 0, the charset of ID N is not a safe character set.
     Such a character set is not encoded when CODING_ISO_FLAG_SAFE is
     set.  */
  unsigned char safe_charsets[MAX_CHARSET + 1];

  /* Non-zero means that characters are being composed currently while
     decoding or encoding.  See macros COMPOSING_XXXX above for the
     meaing of each non-zero value.  */
  int composing;

  /* Detailed information specific to each type of coding system.  */
  union spec
    {
      struct iso2022_spec iso2022;
      struct ccl_spec ccl;	/* Defined in ccl.h.  */
    } spec;

  /* Index number of coding category of the coding system.  */
  int category_idx;

  /* How may heading bytes we can skip for decoding.  This is set to
     -1 in setup_coding_system, and updated by detect_coding.  So,
     when this is equal to the byte length of the text being
     converted, we can skip the actual conversion process.  */
  int heading_ascii;

  /* The following members are set by encoding/decoding routine.  */
  int produced, produced_char, consumed, consumed_char;

  /* Encoding routines set this to 1 when they produce a byte sequence
     which can be parsed as a multibyte character.  Decoding routines
     set this to 1 when they encounter an invalid code and, as the
     result, produce an unexpected multibyte character.  */
  int fake_multibyte;

  /* The following members are all Lisp symbols.  We don't have to
     protect them from GC because the current garbage collection
     doesn't relocate Lisp symbols.  But, when it is changed, we must
     find a way to protect them.  */

  /* Backward pointer to the Lisp symbol of the coding system.  */
  Lisp_Object symbol;

  /* Lisp function (symbol) to be called after decoding to do
     additional conversion, or nil.  */
  Lisp_Object post_read_conversion;

  /* Lisp function (symbol) to be called before encoding to do
     additional conversion, or nil.  */
  Lisp_Object pre_write_conversion;

  /* Character translation tables to look up, or nil.  */
  Lisp_Object translation_table_for_decode;
  Lisp_Object translation_table_for_encode;
};

#define CODING_REQUIRE_FLUSHING_MASK	1
#define CODING_REQUIRE_DECODING_MASK	2
#define CODING_REQUIRE_ENCODING_MASK	4
#define CODING_REQUIRE_DETECTION_MASK	8

/* Return 1 if the coding system CODING requires specific code to be
   attached at the tail of converted text.  */
#define CODING_REQUIRE_FLUSHING(coding) \
  ((coding)->common_flags & CODING_REQUIRE_FLUSHING_MASK)

/* Return 1 if the coding system CODING requires code conversion on
   decoding.  */
#define CODING_REQUIRE_DECODING(coding)	\
  ((coding)->common_flags & CODING_REQUIRE_DECODING_MASK)

/* Return 1 if the coding system CODING requires code conversion on
   encoding.  */
#define CODING_REQUIRE_ENCODING(coding)	\
  ((coding)->common_flags & CODING_REQUIRE_ENCODING_MASK)

/* Return 1 if the coding system CODING requires some kind of code
   detection.  */
#define CODING_REQUIRE_DETECTION(coding) \
  ((coding)->common_flags & CODING_REQUIRE_DETECTION_MASK)

#define CODING_MAY_REQUIRE_DECODING(coding)	\
  ((coding)->common_flags			\
   & (CODING_REQUIRE_DETECTION_MASK | CODING_REQUIRE_DECODING_MASK))

/* Index for each coding category in `coding_category_table' */
#define CODING_CATEGORY_IDX_EMACS_MULE	0
#define CODING_CATEGORY_IDX_SJIS	1
#define CODING_CATEGORY_IDX_ISO_7	2
#define CODING_CATEGORY_IDX_ISO_7_TIGHT	3
#define CODING_CATEGORY_IDX_ISO_8_1	4
#define CODING_CATEGORY_IDX_ISO_8_2	5
#define CODING_CATEGORY_IDX_ISO_7_ELSE	6
#define CODING_CATEGORY_IDX_ISO_8_ELSE	7
#define CODING_CATEGORY_IDX_CCL		8
#define CODING_CATEGORY_IDX_BIG5	9
#define CODING_CATEGORY_IDX_RAW_TEXT	10
#define CODING_CATEGORY_IDX_BINARY	11
#define CODING_CATEGORY_IDX_MAX		12

/* Definitions of flag bits returned by the function
   detect_coding_mask ().  */
#define CODING_CATEGORY_MASK_EMACS_MULE	(1 << CODING_CATEGORY_IDX_EMACS_MULE)
#define CODING_CATEGORY_MASK_SJIS	(1 << CODING_CATEGORY_IDX_SJIS)
#define CODING_CATEGORY_MASK_ISO_7	(1 << CODING_CATEGORY_IDX_ISO_7)
#define CODING_CATEGORY_MASK_ISO_7_TIGHT (1 << CODING_CATEGORY_IDX_ISO_7_TIGHT)
#define CODING_CATEGORY_MASK_ISO_8_1	(1 << CODING_CATEGORY_IDX_ISO_8_1)
#define CODING_CATEGORY_MASK_ISO_8_2	(1 << CODING_CATEGORY_IDX_ISO_8_2)
#define CODING_CATEGORY_MASK_ISO_7_ELSE	(1 << CODING_CATEGORY_IDX_ISO_7_ELSE)
#define CODING_CATEGORY_MASK_ISO_8_ELSE	(1 << CODING_CATEGORY_IDX_ISO_8_ELSE)
#define CODING_CATEGORY_MASK_CCL	(1 << CODING_CATEGORY_IDX_CCL)
#define CODING_CATEGORY_MASK_BIG5	(1 << CODING_CATEGORY_IDX_BIG5)
#define CODING_CATEGORY_MASK_RAW_TEXT	(1 << CODING_CATEGORY_IDX_RAW_TEXT)
#define CODING_CATEGORY_MASK_BINARY	(1 << CODING_CATEGORY_IDX_BINARY)

/* This value is returned if detect_coding_mask () find nothing other
   than ASCII characters.  */
#define CODING_CATEGORY_MASK_ANY  	\
  (  CODING_CATEGORY_MASK_EMACS_MULE	\
   | CODING_CATEGORY_MASK_SJIS	  	\
   | CODING_CATEGORY_MASK_ISO_7	  	\
   | CODING_CATEGORY_MASK_ISO_7_TIGHT  	\
   | CODING_CATEGORY_MASK_ISO_8_1 	\
   | CODING_CATEGORY_MASK_ISO_8_2 	\
   | CODING_CATEGORY_MASK_ISO_7_ELSE	\
   | CODING_CATEGORY_MASK_ISO_8_ELSE	\
   | CODING_CATEGORY_MASK_CCL		\
   | CODING_CATEGORY_MASK_BIG5)

#define CODING_CATEGORY_MASK_ISO_7BIT \
  (CODING_CATEGORY_MASK_ISO_7 | CODING_CATEGORY_MASK_ISO_7_TIGHT)

#define CODING_CATEGORY_MASK_ISO_8BIT \
  (CODING_CATEGORY_MASK_ISO_8_1 | CODING_CATEGORY_MASK_ISO_8_2)

#define CODING_CATEGORY_MASK_ISO_SHIFT \
  (CODING_CATEGORY_MASK_ISO_7_ELSE | CODING_CATEGORY_MASK_ISO_8_ELSE)

#define CODING_CATEGORY_MASK_ISO	\
  (  CODING_CATEGORY_MASK_ISO_7BIT	\
   | CODING_CATEGORY_MASK_ISO_SHIFT	\
   | CODING_CATEGORY_MASK_ISO_8BIT)

/* Macros to decode or encode a character of JISX0208 in SJIS.  S1 and
   S2 are the 1st and 2nd position-codes of JISX0208 in SJIS coding
   system.  C1 and C2 are the 1st and 2nd position codes of Emacs'
   internal format.  */

#define DECODE_SJIS(s1, s2, c1, c2)		  	\
  do {						  	\
    if (s2 >= 0x9F)				  	\
      c1 = s1 * 2 - (s1 >= 0xE0 ? 0x160 : 0xE0),  	\
      c2 = s2 - 0x7E;				  	\
    else					  	\
      c1 = s1 * 2 - ((s1 >= 0xE0) ? 0x161 : 0xE1),	\
      c2 = s2 - ((s2 >= 0x7F) ? 0x20 : 0x1F);	  	\
  } while (0)

#define ENCODE_SJIS(c1, c2, s1, s2)			\
  do {							\
    if (c1 & 1)						\
      s1 = c1 / 2 + ((c1 < 0x5F) ? 0x71 : 0xB1),	\
      s2 = c2 + ((c2 >= 0x60) ? 0x20 : 0x1F);		\
    else						\
      s1 = c1 / 2 + ((c1 < 0x5F) ? 0x70 : 0xB0),	\
      s2 = c2 + 0x7E;					\
  } while (0)

/* Encode the file name NAME using the specified coding system
   for file names, if any.  */
#define ENCODE_FILE(name)						   \
  (! NILP (Vfile_name_coding_system)					   \
   && XFASTINT (Vfile_name_coding_system) != 0				   \
   ? code_convert_string_norecord (name, Vfile_name_coding_system, 1)	   \
   : (! NILP (Vdefault_file_name_coding_system)				   \
      && XFASTINT (Vdefault_file_name_coding_system) != 0		   \
      ? code_convert_string_norecord (name, Vdefault_file_name_coding_system, 1) \
      : name))

/* Decode the file name NAME using the specified coding system
   for file names, if any.  */
#define DECODE_FILE(name)						   \
  (! NILP (Vfile_name_coding_system)					   \
   && XFASTINT (Vfile_name_coding_system) != 0				   \
   ? code_convert_string_norecord (name, Vfile_name_coding_system, 0)	   \
   : (! NILP (Vdefault_file_name_coding_system)				   \
      && XFASTINT (Vdefault_file_name_coding_system) != 0		   \
      ? code_convert_string_norecord (name, Vdefault_file_name_coding_system, 0) \
      : name))

/* Extern declarations.  */
extern int decode_coding P_ ((struct coding_system *, unsigned char *,
			      unsigned char *, int, int));
extern int encode_coding P_ ((struct coding_system *, unsigned char *,
			      unsigned char *, int, int));
extern int code_convert_region P_ ((int, int, int, int, struct coding_system *,
				    int, int));
extern int decoding_buffer_size P_ ((struct coding_system *, int));
extern int encoding_buffer_size P_ ((struct coding_system *, int));
extern void detect_coding P_ ((struct coding_system *, unsigned char *, int));
extern void detect_eol P_ ((struct coding_system *, unsigned char *, int));
extern int conversion_buffer_size;
extern char *conversion_buffer;
extern char *get_conversion_buffer P_ ((int));
extern int setup_coding_system P_ ((Lisp_Object, struct coding_system *));
extern void setup_raw_text_coding_system P_ ((struct coding_system *));
extern Lisp_Object Qcoding_system, Qeol_type, Qcoding_category_index;
extern Lisp_Object Qraw_text, Qemacs_mule;
extern Lisp_Object Qbuffer_file_coding_system;
extern Lisp_Object Vcoding_category_list;

extern Lisp_Object Qtranslation_table;
extern Lisp_Object Qtranslation_table_id;

/* Mnemonic character to indicate each type of end-of-line.  */
extern int eol_mnemonic_unix, eol_mnemonic_dos, eol_mnemonic_mac;
/* Mnemonic character to indicate type of end-of-line is not yet decided.  */
extern int eol_mnemonic_undecided;

#ifdef emacs
extern Lisp_Object Qfile_coding_system;
extern Lisp_Object Qcall_process, Qcall_process_region, Qprocess_argument;
extern Lisp_Object Qstart_process, Qopen_network_stream;

/* Coding-system for reading files and receiving data from process.  */
extern Lisp_Object Vcoding_system_for_read;
/* Coding-system for writing files and sending data to process.  */
extern Lisp_Object Vcoding_system_for_write;
/* Coding-system actually used in the latest I/O.  */
extern Lisp_Object Vlast_coding_system_used;

/* If non-zero, process buffer inherits the coding system used to decode
   the subprocess output.  */
extern int inherit_process_coding_system;

/* Coding-system to be used for encoding terminal output.  This
   structure contains information of a coding-system specified by the
   function `set-terminal-coding-system'.  */
extern struct coding_system terminal_coding;

/* Coding system to be used to encode text for terminal display when
   terminal coding system is nil.  */
extern struct coding_system safe_terminal_coding;

/* Coding-system of what is sent from terminal keyboard.  This
   structure contains information of a coding-system specified by the
   function `set-keyboard-coding-system'.  */
extern struct coding_system keyboard_coding;

/* Default coding system to be used to write a file.  */
extern struct coding_system default_buffer_file_coding;

/* Default coding systems used for process I/O.  */
extern Lisp_Object Vdefault_process_coding_system;

/* Function to call to force a user to force select a propert coding
   system.  */
extern Lisp_Object Vselect_safe_coding_system_function;

/* Coding system for file names, or nil if none.  */
extern Lisp_Object Vfile_name_coding_system;

/* Coding system for file names used only when
   Vfile_name_coding_system is nil.  */
extern Lisp_Object Vdefault_file_name_coding_system;
#endif

#endif /* _CODING_H */
