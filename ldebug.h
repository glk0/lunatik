/*
** $Id: ldebug.h,v 1.9 2001/02/09 18:37:33 roberto Exp roberto $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/

#ifndef ldebug_h
#define ldebug_h


#include "lstate.h"
#include "luadebug.h"


void luaG_typeerror (lua_State *L, StkId o, const char *op);
void luaG_binerror (lua_State *L, StkId p1, int t, const char *op);
int luaG_getline (int *lineinfo, int pc, int refline, int *refi);
void luaG_ordererror (lua_State *L, const TObject *p1, const TObject *p2);
int luaG_checkcode (lua_State *L, const Proto *pt);


#endif
