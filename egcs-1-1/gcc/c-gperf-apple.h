/* C code produced by gperf version 2.5 (GNU C++ version)  */
/* Command-line: gperf -p -j1 -i 1 -g -o -t -G -N is_reserved_word -k1,3,$  */
/* Command-line: gperf -p -j1 -i 1 -g -o -t -N is_reserved_word -k1,3,$ c-parse.gperf  */ 
struct resword { char *name; short token; enum rid rid; };

#define TOTAL_KEYWORDS 88
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 20
#define MIN_HASH_VALUE 12
#define MAX_HASH_VALUE 191
/* maximum key range = 180, duplicates = 0 */

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
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192,   3, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
     192, 192, 192, 192, 192,   1, 192, 110,   8,  15,
      20,   6,  48,  34,   4,  57, 192,   4,  30,  19,
      68,  11,  83, 192,   2,  13,   1,  61,  34,   7,
      34,   8,   1, 192, 192, 192, 192, 192,
    };
  register int hval = len;

  switch (hval)
    {
      default:
      case 3:
        hval += asso_values[str[2]];
      case 2:
      case 1:
        hval += asso_values[str[0]];
        break;
    }
  return hval + asso_values[str[len - 1]];
}

static struct resword wordlist[] =
{
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, 
      {"__real__",  REALPART, NORID},
      {"__typeof__",  TYPEOF, NORID},
      {"",}, {"",}, 
      {"out",  TYPE_QUAL, RID_OUT},
      {"",}, {"",}, 
      {"@private",  PRIVATE, NORID},
      {"@selector",  SELECTOR, NORID},
      {"__extension__",  EXTENSION, NORID},
      {"struct",  STRUCT, NORID},
      {"break",  BREAK, NORID},
      {"__const",  TYPE_QUAL, RID_CONST},
      {"__signed__",  TYPESPEC, RID_SIGNED},
      {"__const__",  TYPE_QUAL, RID_CONST},
      {"@defs",  DEFS, NORID},
      {"__complex__",  TYPESPEC, RID_COMPLEX},
      {"else",  ELSE, NORID},
      {"short",  TYPESPEC, RID_SHORT},
      {"oneway",  TYPE_QUAL, RID_ONEWAY},
      {"__direct__",  SCSPEC, RID_DIRECT},
      {"do",  DO, NORID},
      {"",}, 
      {"@protected",  PROTECTED, NORID},
      {"",}, 
      {"bycopy",  TYPE_QUAL, RID_BYCOPY},
      {"case",  CASE, NORID},
      {"__real",  REALPART, NORID},
      {"",}, 
      {"__label__",  LABEL, NORID},
      {"__signed",  TYPESPEC, RID_SIGNED},
      {"",}, 
      {"@protocol",  PROTOCOL, NORID},
      {"__vector",  TYPESPEC, RID_VECTOR},
      {"register",  SCSPEC, RID_REGISTER},
      {"@compatibility_alias",  ALIAS, NORID},
      {"__volatile__",  TYPE_QUAL, RID_VOLATILE},
      {"",}, 
      {"goto",  GOTO, NORID},
      {"__volatile",  TYPE_QUAL, RID_VOLATILE},
      {"@class",  CLASS, NORID},
      {"bool",  TYPESPEC, RID_BOOL},
      {"",}, 
      {"for",  FOR, NORID},
      {"",}, 
      {"vector",  TYPESPEC, RID_VECTOR},
      {"__typeof",  TYPEOF, NORID},
      {"__complex",  TYPESPEC, RID_COMPLEX},
      {"",}, {"",}, 
      {"int",  TYPESPEC, RID_INT},
      {"byref",  TYPE_QUAL, RID_BYREF},
      {"",}, 
      {"float",  TYPESPEC, RID_FLOAT},
      {"",}, 
      {"__imag__",  IMAGPART, NORID},
      {"sizeof",  SIZEOF, NORID},
      {"__inline__",  SCSPEC, RID_INLINE},
      {"__iterator",  SCSPEC, RID_ITERATOR},
      {"__iterator__",  SCSPEC, RID_ITERATOR},
      {"__inline",  SCSPEC, RID_INLINE},
      {"signed",  TYPESPEC, RID_SIGNED},
      {"inout",  TYPE_QUAL, RID_INOUT},
      {"while",  WHILE, NORID},
      {"default",  DEFAULT, NORID},
      {"return",  RETURN, NORID},
      {"volatile",  TYPE_QUAL, RID_VOLATILE},
      {"id",  OBJECTNAME, RID_ID},
      {"switch",  SWITCH, NORID},
      {"extern",  SCSPEC, RID_EXTERN},
      {"",}, {"",}, 
      {"@encode",  ENCODE, NORID},
      {"",}, 
      {"@public",  PUBLIC, NORID},
      {"@interface",  INTERFACE, NORID},
      {"",}, 
      {"const",  TYPE_QUAL, RID_CONST},
      {"enum",  ENUM, NORID},
      {"",}, {"",}, 
      {"double",  TYPESPEC, RID_DOUBLE},
      {"",}, 
      {"@end",  END, NORID},
      {"",}, 
      {"continue",  CONTINUE, NORID},
      {"__imag",  IMAGPART, NORID},
      {"inline",  SCSPEC, RID_INLINE},
      {"",}, {"",}, 
      {"unsigned",  TYPESPEC, RID_UNSIGNED},
      {"__private_extern__",  TYPE_QUAL, RID_PRIVATE_EXTERN},
      {"",}, 
      {"@implementation",  IMPLEMENTATION, NORID},
      {"",}, 
      {"if",  IF, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"void",  TYPESPEC, RID_VOID},
      {"",}, {"",}, {"",}, 
      {"__asm__",  ASM_KEYWORD, NORID},
      {"",}, 
      {"__pixel",  TYPESPEC, RID_PIXEL},
      {"",}, 
      {"__alignof__",  ALIGNOF, NORID},
      {"",}, 
      {"__attribute__",  ATTRIBUTE, NORID},
      {"auto",  SCSPEC, RID_AUTO},
      {"in",  TYPE_QUAL, RID_IN},
      {"__attribute",  ATTRIBUTE, NORID},
      {"",}, {"",}, 
      {"char",  TYPESPEC, RID_CHAR},
      {"",}, {"",}, {"",}, 
      {"__asm",  ASM_KEYWORD, NORID},
      {"long",  TYPESPEC, RID_LONG},
      {"",}, 
      {"typeof",  TYPEOF, NORID},
      {"typedef",  SCSPEC, RID_TYPEDEF},
      {"vec_step",  VEC_STEP, NORID},
      {"",}, {"",}, {"",}, 
      {"static",  SCSPEC, RID_STATIC},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"asm",  ASM_KEYWORD, NORID},
      {"pixel",  TYPESPEC, RID_PIXEL},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"__alignof",  ALIGNOF, NORID},
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, {"",}, 
      {"",}, {"",}, {"",}, {"",}, 
      {"union",  UNION, NORID},
};

#ifdef __GNUC__
inline
#endif
struct resword *
is_reserved_word (str, len)
     register char *str;
     register unsigned int len;
{
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
