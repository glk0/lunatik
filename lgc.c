/*
** $Id: lgc.c,v 1.89 2001/02/20 18:15:33 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#include "lua.h"

#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


/*
** optional "lock" for GC
** (when Lua calls GC tag methods it unlocks the regular lock)
*/
#ifndef LUA_LOCKGC
#define LUA_LOCKGC(L)		{
#endif

#ifndef LUA_UNLOCKGC
#define LUA_UNLOCKGC(L)		}
#endif


typedef struct GCState {
  Hash *tmark;  /* list of marked tables to be visited */
  Closure *cmark;  /* list of marked closures to be visited */
} GCState;



static void markobject (GCState *st, TObject *o);


/* mark a string; marks larger than 1 cannot be changed */
#define strmark(s)    {if ((s)->marked == 0) (s)->marked = 1;}



static void protomark (Proto *f) {
  if (!f->marked) {
    int i;
    f->marked = 1;
    strmark(f->source);
    for (i=0; i<f->sizekstr; i++)
      strmark(f->kstr[i]);
    for (i=0; i<f->sizekproto; i++)
      protomark(f->kproto[i]);
    for (i=0; i<f->sizelocvars; i++)  /* mark local-variable names */
      strmark(f->locvars[i].varname);
  }
}


static void markclosure (GCState *st, Closure *cl) {
  if (!ismarked(cl)) {
    if (!cl->isC) {
      lua_assert(cl->nupvalues == cl->f.l->nupvalues);
      protomark(cl->f.l);
    }
    cl->mark = st->cmark;  /* chain it for later traversal */
    st->cmark = cl;
  }
}


static void marktable (GCState *st, Hash *h) {
  if (!ismarked(h)) {
    h->mark = st->tmark;  /* chain it in list of marked */
    st->tmark = h;
  }
}


static void markobject (GCState *st, TObject *o) {
  switch (ttype(o)) {
    case LUA_TUSERDATA:  case LUA_TSTRING:
      strmark(tsvalue(o));
      break;
    case LUA_TMARK:
      markclosure(st, infovalue(o)->func);
      break;
    case LUA_TFUNCTION:
      markclosure(st, clvalue(o));
      break;
    case LUA_TTABLE: {
      marktable(st, hvalue(o));
      break;
    }
    default: break;  /* numbers, etc */
  }
}


static void markstacks (lua_State *L, GCState *st) {
  lua_State *L1 = L;
  do {  /* for each thread */
    StkId o, lim;
    marktable(st, L1->gt);  /* mark table of globals */
    for (o=L1->stack; o<L1->top; o++)
      markobject(st, o);
    lim = (L1->stack_last - L1->top > MAXSTACK) ? L1->top+MAXSTACK
                                              : L1->stack_last;
    for (; o<=lim; o++) setnilvalue(o);
    lua_assert(L1->previous->next == L1 && L1->next->previous == L1);
    L1 = L1->next;
  } while (L1 != L);
}


static void marklock (global_State *G, GCState *st) {
  int i;
  for (i=0; i<G->nref; i++) {
    if (G->refArray[i].st == LOCK)
      markobject(st, &G->refArray[i].o);
  }
}


static void marktagmethods (global_State *G, GCState *st) {
  int t;
  for (t=0; t<G->ntag; t++) {
    struct TM *tm = &G->TMtable[t];
    int e;
    if (tm->name) strmark(tm->name);
    for (e=0; e<TM_N; e++) {
      Closure *cl = tm->method[e];
      if (cl) markclosure(st, cl);
    }
  }
}


static void traverseclosure (GCState *st, Closure *f) {
  int i;
  for (i=0; i<f->nupvalues; i++)  /* mark its upvalues */
    markobject(st, &f->upvalue[i]);
}


static void traversetable (GCState *st, Hash *h) {
  int i;
  for (i=0; i<h->size; i++) {
    Node *n = node(h, i);
    if (ttype(val(n)) == LUA_TNIL) {
      if (ttype_key(n) != LUA_TNIL)
        n->key_value.ts = NULL;  /* dead key; remove it */
    }
    else {
      lua_assert(ttype_key(n) != LUA_TNIL);
      if (ttype_key(n) != LUA_TNUMBER) {
        TObject o;
        setkey2obj(&o, n);
        markobject(st, &o);
      }
      markobject(st, &n->val);
    }
  }
}


static void markall (lua_State *L) {
  GCState st;
  st.cmark = NULL;
  st.tmark = NULL;
  marktagmethods(G(L), &st);  /* mark tag methods */
  markstacks(L, &st); /* mark all stacks */
  marklock(G(L), &st); /* mark locked objects */
  marktable(&st, G(L)->type2tag);
  for (;;) {  /* mark tables and closures */
    if (st.cmark) {
      Closure *f = st.cmark;  /* get first closure from list */
      st.cmark = f->mark;  /* remove it from list */
      traverseclosure(&st, f);
    }
    else if (st.tmark) {
      Hash *h = st.tmark;  /* get first table from list */
      st.tmark = h->mark;  /* remove it from list */
      traversetable(&st, h);
    }
    else break;  /* nothing else to mark */
  }
}


static int hasmark (const TObject *o) {
  /* valid only for locked objects */
  switch (ttype(o)) {
    case LUA_TSTRING: case LUA_TUSERDATA:
      return tsvalue(o)->marked;
    case LUA_TTABLE:
      return ismarked(hvalue(o));
    case LUA_TFUNCTION:
      return ismarked(clvalue(o));
    default:  /* number */
      return 1;
  }
}


/* macro for internal debugging; check if a link of free refs is valid */
#define VALIDLINK(L, st,n)      (NONEXT <= (st) && (st) < (n))

static void invalidaterefs (global_State *G) {
  int n = G->nref;
  int i;
  for (i=0; i<n; i++) {
    struct Ref *r = &G->refArray[i];
    if (r->st == HOLD && !hasmark(&r->o))
      r->st = COLLECTED;
    lua_assert((r->st == LOCK && hasmark(&r->o)) ||
               (r->st == HOLD && hasmark(&r->o)) ||
                r->st == COLLECTED ||
                r->st == NONEXT ||
               (r->st < n && VALIDLINK(L, G->refArray[r->st].st, n)));
  }
  lua_assert(VALIDLINK(L, G->refFree, n));
}



static void collectproto (lua_State *L) {
  Proto **p = &G(L)->rootproto;
  Proto *next;
  while ((next = *p) != NULL) {
    if (next->marked) {
      next->marked = 0;
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeproto(L, next);
    }
  }
}


static void collectclosure (lua_State *L) {
  Closure **p = &G(L)->rootcl;
  Closure *next;
  while ((next = *p) != NULL) {
    if (ismarked(next)) {
      next->mark = next;  /* unmark */
      p = &next->next;
    }
    else {
      *p = next->next;
      luaF_freeclosure(L, next);
    }
  }
}


static void collecttable (lua_State *L) {
  Hash **p = &G(L)->roottable;
  Hash *next;
  while ((next = *p) != NULL) {
    if (ismarked(next)) {
      next->mark = next;  /* unmark */
      p = &next->next;
    }
    else {
      *p = next->next;
      luaH_free(L, next);
    }
  }
}


static void checktab (lua_State *L, stringtable *tb) {
  if (tb->nuse < (ls_nstr)(tb->size/4) && tb->size > MINPOWER2)
    luaS_resize(L, tb, tb->size/2);  /* table is too big */
}


static void collectstrings (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->strt.size; i++) {  /* for each list */
    TString **p = &G(L)->strt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      if (next->marked && !all) {  /* preserve? */
        if (next->marked < FIXMARK)  /* does not change FIXMARKs */
          next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        *p = next->nexthash;
        G(L)->strt.nuse--;
        luaM_free(L, next, sizestring(next->len));
      }
    }
  }
  checktab(L, &G(L)->strt);
}


static void collectudata (lua_State *L, int all) {
  int i;
  for (i=0; i<G(L)->udt.size; i++) {  /* for each list */
    TString **p = &G(L)->udt.hash[i];
    TString *next;
    while ((next = *p) != NULL) {
      lua_assert(next->marked <= 1);
      if (next->marked && !all) {  /* preserve? */
        next->marked = 0;
        p = &next->nexthash;
      } 
      else {  /* collect */
        int tag = next->u.d.tag;
        *p = next->nexthash;
        next->nexthash = G(L)->TMtable[tag].collected;  /* chain udata */
        G(L)->TMtable[tag].collected = next;
        G(L)->udt.nuse--;
      }
    }
  }
  checktab(L, &G(L)->udt);
}


#define MINBUFFER	256
static void checkMbuffer (lua_State *L) {
  if (G(L)->Mbuffsize > MINBUFFER*2) {  /* is buffer too big? */
    size_t newsize = G(L)->Mbuffsize/2;  /* still larger than MINBUFFER */
    luaM_reallocvector(L, G(L)->Mbuffer, G(L)->Mbuffsize, newsize, char);
    G(L)->Mbuffsize = newsize;
  }
}


static void callgcTM (lua_State *L, const TObject *obj) {
  Closure *tm = luaT_gettmbyObj(G(L), obj, TM_GC);
  if (tm != NULL) {
    int oldah = L->allowhooks;
    L->allowhooks = 0;  /* stop debug hooks during GC tag methods */
    luaD_checkstack(L, 2);
    setclvalue(L->top, tm);
    setobj(L->top+1, obj);
    L->top += 2;
    luaD_call(L, L->top-2, 0);
    L->allowhooks = oldah;  /* restore hooks */
  }
}


static void callgcTMudata (lua_State *L) {
  int tag;
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* avoid GC during tag methods */
  for (tag=G(L)->ntag-1; tag>=0; tag--) {  /* for each tag (in reverse order) */
    TString *udata;
    while ((udata = G(L)->TMtable[tag].collected) != NULL) {
      TObject obj;
      G(L)->TMtable[tag].collected = udata->nexthash;  /* remove it from list */
      setuvalue(&obj, udata);
      callgcTM(L, &obj);
      luaM_free(L, udata, sizeudata(udata->len));
    }
  }
}


void luaC_collect (lua_State *L, int all) {
  LUA_LOCKGC(L);
  collectudata(L, all);
  callgcTMudata(L);
  collectstrings(L, all);
  collecttable(L);
  collectproto(L);
  collectclosure(L);
  LUA_UNLOCKGC(L);
}


void luaC_collectgarbage (lua_State *L) {
  markall(L);
  invalidaterefs(G(L));  /* check unlocked references */
  luaC_collect(L, 0);
  checkMbuffer(L);
  G(L)->GCthreshold = 2*G(L)->nblocks;  /* set new threshold */
  callgcTM(L, &luaO_nilobject);
}


void luaC_checkGC (lua_State *L) {
  if (G(L)->nblocks >= G(L)->GCthreshold)
    luaC_collectgarbage(L);
}

