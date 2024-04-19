/* C code produced by gperf version 2.5 (GNU C++ version) */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,7,$  */
/* Command-line: gperf -p -j1 -g -o -t -N is_reserved_word -k1,4,$,7 gplus.gperf  */
struct resword { char *name; short token; enum rid rid;};

#define TOTAL_KEYWORDS 108
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 16
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 237
/* maximum key range = 234, duplicates = 0 */

#ifdef __GNUC__
inline
#endif
static unsigned int
hash (str, len)
     register char *str;
     register int unsigned len;
{
  static unsigned char asso_values[] =
    {
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238, 238, 238, 238, 238, 238,
     238, 238, 238, 238, 238,   0, 238,  84,  17,  29,
      35,   0,  59,   6,   0, 104, 238,   4,   0,  64,
      16,  71,  32,   4,  33,   5,   6,  90,  11,  10,
       3,  10, 238, 238, 238, 238, 238, 238,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 7:
        hval += asso_values[str[6]];
      case 6:
      case 5:
      case 4:
        hval += asso_values[str[3]];
      case 3:
      case 2:
      case 1:
        hval += asso_values[str[0]];
        break;
    }
  return hval + asso_values[str[len - 1]];
}

#ifdef __GNUC__
inline
#endif
struct resword *
is_reserved_word (str, len)
     register char *str;
     register unsigned int len;
{
  static struct resword wordlist[] =
    {
      {"",}, {"",}, {"",}, {"",}, 
      {"else",  ELSE, NORID,},
      {"",}, 
      {"__real",  REALPART, NORID},
      {"",}, 
      {"__real__",  REALPART, NORID},
      {"",}, 
      {"true",  CXX_TRUE, NORID,},
      {"",}, 
      {"__asm__",  ASM_KEYWORD, NORID},
      {"xor_eq",  ASSIGN, NORID,},
      {"",}, 
      {"while",  WHILE, NORID,},
      {"long",  TYPESPEC, RID_LONG,},
      {"switch",  SWITCH, NORID,},
      {"",}, 
      {"try",  TRY, NORID,},
      {"this",  THIS, NORID,},
      {"bool",  TYPESPEC, RID_BOOL,},
      {"extern",  SCSPEC, RID_EXTERN,},
      {"",}, 
      {"virtual",  SCSPEC, RID_VIRTUAL,},
      {"not",  '!', NORID,},
      {"not_eq",  EQCOMPARE, NORID,},
      {"__alignof__",  ALIGNOF, NORID},
      {"static_cast",  STATIC_CAST, NORID,},
      {"new",  NEW, NORID,},
      {"",}, {"",}, 
      {"__extension__",  EXTENSION, NORID},
      {"case",  CASE, NORID,},
      {"",}, {"",}, {"",}, 
      {"pixel",  TYPESPEC, RID_PIXEL,},
      {"",}, 
      {"xor",  '^', NORID,},
      {"__inline",  SCSPEC, RID_INLINE},
      {"delete",  DELETE, NORID,},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"",}, 
      {"class",  AGGR, RID_CLASS,},
      {"const",  CV_QUALIFIER, RID_CONST,},
      {"static",  SCSPEC, RID_STATIC,},
      {"typeid",  TYPEID, NORID,},
      {"",}, 
      {"short",  TYPESPEC, RID_SHORT,},
      {"private",  VISSPEC, RID_PRIVATE,},
      {"vec_step",  VEC_STEP, NORID},
      {"template",  TEMPLATE, RID_TEMPLATE,},
      {"",}, {"",}, {"",}, 
      {"vector",  TYPESPEC, RID_VECTOR,},
      {"",}, 
      {"double",  TYPESPEC, RID_DOUBLE,},
      {"",}, {"",}, {"",}, 
      {"signed",  TYPESPEC, RID_SIGNED,},
      {"catch",  CATCH, NORID,},
      {"",}, {"",}, 
      {"compl",  '~', NORID,},
      {"public",  VISSPEC, RID_PUBLIC,},
      {"",}, 
      {"false",  CXX_FALSE, NORID,},
      {"sizeof",  SIZEOF, NORID,},
      {"typeof",  TYPEOF, NORID,},
      {"__imag__",  IMAGPART, NORID},
      {"",}, 
      {"__asm",  ASM_KEYWORD, NORID},
      {"",}, 
      {"__imag",  IMAGPART, NORID},
      {"__wchar_t",  TYPESPEC, RID_WCHAR  /* Unique to ANSI C++ */,},
      {"typename",  TYPENAME_KEYWORD, NORID,},
      {"const_cast",  CONST_CAST, NORID,},
      {"or_eq",  ASSIGN, NORID,},
      {"",}, 
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"__alignof",  ALIGNOF, NORID},
      {"void",  TYPESPEC, RID_VOID,},
      {"__const__",  CV_QUALIFIER, RID_CONST},
      {"__volatile",  CV_QUALIFIER, RID_VOLATILE},
      {"protected",  VISSPEC, RID_PROTECTED,},
      {"__volatile__",  CV_QUALIFIER, RID_VOLATILE},
      {"__const",  CV_QUALIFIER, RID_CONST},
      {"__typeof__",  TYPEOF, NORID},
      {"throw",  THROW, NORID,},
      {"__label__",  LABEL, NORID},
      {"and_eq",  ASSIGN, NORID,},
      {"for",  FOR, NORID,},
      {"__null",  CONSTANT, RID_NULL},
      {"",}, {"",}, 
      {"char",  TYPESPEC, RID_CHAR,},
      {"friend",  SCSPEC, RID_FRIEND,},
      {"",}, {"",}, 
      {"volatile",  CV_QUALIFIER, RID_VOLATILE,},
      {"reinterpret_cast",  REINTERPRET_CAST, NORID,},
      {"",}, 
      {"or",  OROR, NORID,},
      {"struct",  AGGR, RID_RECORD,},
      {"do",  DO, NORID,},
      {"namespace",  NAMESPACE, NORID,},
      {"break",  BREAK, NORID,},
      {"__pixel",  TYPESPEC, RID_PIXEL},
      {"__vector",  TYPESPEC, RID_VECTOR},
      {"int",  TYPESPEC, RID_INT,},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"",}, {"",}, 
      {"using",  USING, NORID,},
      {"explicit",  SCSPEC, RID_EXPLICIT,},
      {"",}, 
      {"signature",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"__attribute",  ATTRIBUTE, NORID},
      {"and",  ANDAND, NORID,},
      {"__attribute__",  ATTRIBUTE, NORID},
      {"",}, {"",}, 
      {"bitor",  '|', NORID,},
      {"",}, {"",}, {"",}, {"",}, 
      {"typedef",  SCSPEC, RID_TYPEDEF,},
      {"enum",  ENUM, NORID,},
      {"continue",  CONTINUE, NORID,},
      {"",}, {"",}, {"",}, {"",}, 
      {"default",  DEFAULT, NORID,},
      {"",}, 
      {"sigof",  SIGOF, NORID		/* Extension */,},
      {"",}, 
      {"bitand",  '&', NORID,},
      {"",}, {"",}, 
      {"return",  RETURN, NORID,},
      {"",}, 
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"__typeof",  TYPEOF, NORID},
      {"",}, {"",}, 
      {"asm",  ASM_KEYWORD, NORID,},
      {"goto",  GOTO, NORID,},
      {"",}, 
      {"float",  TYPESPEC, RID_FLOAT,},
      {"mutable",  SCSPEC, RID_MUTABLE,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"if",  IF, NORID,},
      {"dynamic_cast",  DYNAMIC_CAST, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__sigof__",  SIGOF, NORID		/* Extension */,},
      {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"register",  SCSPEC, RID_REGISTER,},
      {"",}, {"",}, {"",}, 
      {"union",  AGGR, RID_UNION,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      
      {"__signature__",  AGGR, RID_SIGNATURE	/* Extension */,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, 
      {"inline",  SCSPEC, RID_INLINE,},
      {"",}, 
      {"operator",  OPERATOR, NORID,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"auto",  SCSPEC, RID_AUTO,},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED,},
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register char *s = wordlist[key].name;

          if (*s == *str && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
