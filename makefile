#
## $Id: makefile,v 1.9 1998/01/02 17:46:32 roberto Exp roberto $
## Makefile
## See Copyright Notice in lua.h
#


#CONFIGURATION

# define (undefine) POPEN if your system (does not) support piped I/O
#
# define (undefine) _POSIX_SOURCE if your system is (not) POSIX compliant
#
# define (undefine) OLD_ANSI if your system does NOT have some new ANSI
#   facilities (e.g. strerror, locale.h, memmove). Although they are ANSI,
#   SunOS does not comply; so, add "-DOLD_ANSI" on SunOS
#
# define LUA_COMPAT2_5 if yous system does need to be compatible with
# version 2.5 (or older)

CONFIG = -DPOPEN -D_POSIX_SOURCE
#CONFIG = -DLUA_COMPAT2_5 -DOLD_ANSI -DDEBUG


# Compilation parameters
CC = gcc
CWARNS = -Wall -Wmissing-prototypes -Wshadow -pedantic -Wpointer-arith -Wcast-align -Waggregate-return
CFLAGS = $(CONFIG) $(CWARNS) -ansi -O2


AR = ar
ARFLAGS	= rvl


# Aplication modules
LUAOBJS = \
	lapi.o \
	lauxlib.o \
	lbuffer.o \
	lbuiltin.o \
	ldo.o \
	lfunc.o \
	lgc.o \
	llex.o \
	lmem.o \
	lobject.o \
	lstate.o \
	lstx.o \
	lstring.o \
	ltable.o \
	ltm.o \
	lvm.o \
	lundump.o \
	lzio.o

LIBOBJS = 	\
	liolib.o \
	lmathlib.o \
	lstrlib.o


lua : lua.o liblua.a liblualib.a
	$(CC) $(CFLAGS) -o $@ lua.o -L. -llua -llualib -lm

liblua.a : $(LUAOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblualib.a : $(LIBOBJS)
	$(AR) $(ARFLAGS) $@  $?
	ranlib $@

liblua.so.1.0 : lua.o
	ld -o liblua.so.1.0 $(LUAOBJS)

lstx.c lstx.h  : lua.stx
	bison -o lstx.c -p luaY_ -d lua.stx
#	yacc -d lua.stx
#	sed -e 's/yy/luaY_/g' -e 's/malloc\.h/stdlib\.h/g' y.tab.c > lstx.c
#	sed -e 's/yy/luaY_/g' y.tab.h > lstx.h

clear	:
	rcsclean
	rm -f *.o
	rm -f lstx.c lstx.h
	co lua.h lualib.h luadebug.h


%.h : RCS/%.h,v
	co $@

%.c : RCS/%.c,v
	co $@

lapi.o: lapi.c lapi.h lua.h lobject.h lauxlib.h ldo.h lstate.h lfunc.h \
 lgc.h lmem.h lstring.h ltable.h ltm.h luadebug.h lvm.h
lauxlib.o: lauxlib.c lauxlib.h lua.h luadebug.h
lbuffer.o: lbuffer.c lauxlib.h lua.h lmem.h lstate.h lobject.h
lbuiltin.o: lbuiltin.c lapi.h lua.h lobject.h lauxlib.h lbuiltin.h \
 ldo.h lstate.h lfunc.h lmem.h lstring.h ltable.h ltm.h
ldo.o: ldo.c ldo.h lobject.h lua.h lstate.h lfunc.h lgc.h lmem.h \
 lparser.h lzio.h ltm.h luadebug.h lundump.h lvm.h
lfunc.o: lfunc.c lfunc.h lobject.h lua.h lmem.h lstate.h
lgc.o: lgc.c ldo.h lobject.h lua.h lstate.h lfunc.h lgc.h lmem.h \
 lstring.h ltable.h ltm.h
liolib.o: liolib.c lauxlib.h lua.h luadebug.h lualib.h
llex.o: llex.c lauxlib.h lua.h llex.h lobject.h lzio.h lmem.h \
 lparser.h lstate.h lstring.h lstx.h luadebug.h
lmathlib.o: lmathlib.c lauxlib.h lua.h lualib.h
lmem.o: lmem.c lmem.h lstate.h lobject.h lua.h
lobject.o: lobject.c lobject.h lua.h
lstate.o: lstate.c lbuiltin.h ldo.h lobject.h lua.h lstate.h lfunc.h \
 lgc.h llex.h lzio.h lmem.h lstring.h ltable.h ltm.h
lstring.o: lstring.c lmem.h lobject.h lua.h lstate.h lstring.h
lstrlib.o: lstrlib.c lauxlib.h lua.h lualib.h
lstx.o: lstx.c lauxlib.h lua.h ldo.h lobject.h lstate.h lfunc.h llex.h \
 lzio.h lmem.h lopcodes.h lparser.h lstring.h luadebug.h
ltable.o: ltable.c lauxlib.h lua.h lmem.h lobject.h lstate.h ltable.h
ltm.o: ltm.c lauxlib.h lua.h lmem.h lobject.h lstate.h ltm.h lapi.h
lua.o: lua.c lua.h luadebug.h lualib.h
lundump.o: lundump.c lauxlib.h lua.h lfunc.h lobject.h lmem.h \
 lstring.h lundump.h lzio.h
lvm.o: lvm.c lauxlib.h lua.h ldo.h lobject.h lstate.h lfunc.h lgc.h \
 lmem.h lopcodes.h lstring.h ltable.h ltm.h luadebug.h lvm.h
lzio.o: lzio.c lzio.h
