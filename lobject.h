/*
** $Id: lobject.h,v 1.5 1997/10/16 20:07:40 roberto Exp roberto $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/

#ifndef lobject_h
#define lobject_h


#include "lua.h"


#include <limits.h>

#ifndef real
#define real float
#endif

#define Byte lua_Byte	/* some systems have Byte as a predefined type */
typedef unsigned char  Byte;  /* unsigned 8 bits */

#define Word lua_Word	/* some systems have Word as a predefined type */
typedef unsigned short Word;  /* unsigned 16 bits */

#define MAX_WORD  (USHRT_MAX-2)  /* maximum value of a word (-2 for safety) */
#define MAX_INT   (INT_MAX-2)  /* maximum value of a int (-2 for safety) */

typedef unsigned int IntPoint; /* unsigned with same size as a pointer (for hashing) */


/*
** Lua TYPES
** WARNING: if you change the order of this enumeration,
** grep "ORDER LUA_T"
*/
typedef enum {
  LUA_T_USERDATA =  0,  /* tag default for userdata */
  LUA_T_NUMBER   = -1,  /* fixed tag for numbers */
  LUA_T_STRING   = -2,  /* fixed tag for strings */
  LUA_T_ARRAY    = -3,  /* tag default for tables (or arrays) */
  LUA_T_FUNCTION = -4,  /* fixed tag for functions */
  LUA_T_CFUNCTION= -5,  /* fixed tag for Cfunctions */
  LUA_T_NIL      = -6,  /* last "pre-defined" tag */
  LUA_T_PROTO    = -7,
  LUA_T_MARK     = -8,
  LUA_T_CMARK    = -9,
  LUA_T_LINE     = -10
} lua_Type;

#define NUM_TYPES 11
#define NUM_TAGS  7


typedef union {
  lua_CFunction f;  /* LUA_T_CFUNCTION, LUA_T_CMARK */
  real n;  /* LUA_T_NUMBER */
  struct TaggedString *ts;  /* LUA_T_STRING, LUA_T_USERDATA */
  struct TProtoFunc *tf;  /* LUA_T_PROTO */
  struct Closure *cl;  /* LUA_T_FUNCTION, LUA_T_MARK */
  struct Hash *a;  /* LUA_T_ARRAY */
  int i;  /* LUA_T_LINE */
} Value;


typedef struct TObject {
  lua_Type ttype;
  Value value;
} TObject;



/*
** generic header for garbage collector lists
*/
typedef struct GCnode {
  struct GCnode *next;
  int marked;
} GCnode;


/*
** String headers for string table
*/

typedef struct TaggedString {
  GCnode head;
  int constindex;  /* hint to reuse constants (= -1 if this is a userdata) */
  unsigned long hash;
  union {
    TObject globalval;
    struct {
      void *v;  /* if this is a userdata, here is its value */
      int tag;
    } d;
  } u;
  char str[1];   /* \0 byte already reserved */
} TaggedString;




/*
** Function Prototypes
*/
typedef struct TProtoFunc {
  GCnode head;
  Byte *code;  /* ends with opcode ENDCODE */
  int lineDefined;
  TaggedString  *fileName;
  struct TObject *consts;
  int nconsts;
  struct LocVar *locvars;  /* ends with line = -1 */
} TProtoFunc;

typedef struct LocVar {
  TaggedString *varname;           /* NULL signals end of scope */
  int line;
} LocVar;





/* Macros to access structure members */
#define ttype(o)        ((o)->ttype)
#define nvalue(o)       ((o)->value.n)
#define svalue(o)       ((o)->value.ts->str)
#define tsvalue(o)      ((o)->value.ts)
#define clvalue(o)      ((o)->value.cl)
#define avalue(o)       ((o)->value.a)
#define fvalue(o)       ((o)->value.f)
#define tfvalue(o)	((o)->value.tf)


/*
** Closures
*/
typedef struct Closure {
  GCnode head;
  int nelems;  /* not included the first one (always the prototype) */
  TObject consts[1];  /* at least one for prototype */
} Closure;



typedef struct node {
  TObject ref;
  TObject val;
} Node;

typedef struct Hash {
  GCnode head;
  Node *node;
  int nhash;
  int nuse;
  int htag;
} Hash;


/*
** a gross estimation of number of memory "blocks" allocated
** (a block is *roughly* 32 bytes)
*/
extern unsigned long luaO_nblocks;

extern char *luaO_typenames[];

int luaO_equalObj (TObject *t1, TObject *t2);
int luaO_redimension (int oldsize);
int luaO_findstring (char *name, char *list[]);
void luaO_insertlist (GCnode *root, GCnode *node);


#endif
