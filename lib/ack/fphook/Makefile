# Makefile for lib/ack/fphook.

# The ACK ANSI C compiler has an nice trick to reduce the size of programs
# that do not use floating point.  If a program uses floating point then the
# compiler generates an external reference to the label '_fp_hook'.  This makes
# the loader bring in the floating point printing and conversion routines
# '_f_print' and 'strtod' from the library libd.a.  Otherwise two dummy
# routines are found in libc.a.  (The printf and scanf need floating point
# for the %f formats, whether you use them or not.)

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -I..

LIBRARIES = libd libc

libd_OBJECTS = fphook.o

libc_OBJECTS = fltpr.o strtod.o

include ../../Makefile.ack.inc
