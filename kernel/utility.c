/* This file contains a collection of miscellaneous procedures:
 *   panic:	    abort MINIX due to a fatal error
 *   kprintf:	    diagnostic output for the kernel 
 *
 * Changes:
 *   Apr 5, 2006    Xen modifications (Ivan Kelly)
 *   Dec 10, 2004   kernel printing to circular buffer  (Jorrit N. Herder)
 * 
 * This file contains the routines that take care of kernel messages, i.e.,
 * diagnostic output within the kernel. Kernel messages are not directly
 * displayed on the console, because this must be done by the output driver. 
 * Instead, the kernel accumulates characters in a buffer and notifies the
 * output driver when a new message is ready. 
 */

#include <minix/com.h>
#include "kernel.h"
#include <stdarg.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <signal.h>
#include "proc.h"

#define KPUTC_TTY        1
#define KPUTC_XEN        2
#define END_OF_KMESS 	-1

FORWARD _PROTOTYPE(void kputc, (int output_type, int c));
FORWARD _PROTOTYPE(void do_printf, (int output_type, const char *fmt, va_list argp));
FORWARD _PROTOTYPE(void kputc_xen, (int c));
FORWARD _PROTOTYPE(void kputc_tty, (int c));

/*===========================================================================*
 *				panic                                        *
 *===========================================================================*/
PUBLIC void panic(mess,nr)
_CONST char *mess;
int nr;
{
/* The system has run aground of a fatal kernel error. Terminate execution. */
  static int panicking = 0;
  if (panicking ++) return;		/* prevent recursive panics */

  if (mess != NULL) {
    xen_kprintf("\nKernel panic: %s", mess);
    if (nr != NO_NUM)
      xen_kprintf(" %d", nr);
    xen_kprintf("\n", NO_NUM);
  }

  /* Abort MINIX. */
  prepare_shutdown(RBT_PANIC);
}

PUBLIC void kprintf(const char *fmt, ...)
{
	va_list argp;		/* optional arguments */

	va_start(argp, fmt);	/* init variable arguments */

	do_printf(KPUTC_TTY, fmt, argp);

	va_end(argp);		/* end variable arguments */
}

PUBLIC void xen_kprintf(const char *fmt, ...)
{
	va_list argp;		/* optional arguments */

	va_start(argp, fmt);	/* init variable arguments */

	do_printf(KPUTC_XEN, fmt, argp);

	va_end(argp);		/* end variable arguments */
}

/*===========================================================================*
 *				kprintf					     *
 *===========================================================================*/
PUBLIC void kprintf(const char *fmt, ...) 	/* format to be printed */
{
  int c;					/* next character in fmt */
  int d;
  unsigned long u;				/* hold number argument */
  int base;					/* base of number arg */
  int negative;					/* print minus sign */
  static char x2c[] = "0123456789ABCDEF";	/* nr conversion table */
  char ascii[8 * sizeof(long) / 3 + 2];	/* string for ascii number */
  char *s;		/* string to be printed */

  while((c=*fmt++) != 0) {

      if (c == '%') {				/* expect format '%key' */
	  negative = 0;				/* (re)initialize */
	  s = NULL;				/* (re)initialize */
          switch(c = *fmt++) {			/* determine what to do */

          /* Known keys are %d, %u, %x, %s, and %%. This is easily extended 
           * with number types like %b and %o by providing a different base.
           * Number type keys don't set a string to 's', but use the general
           * conversion after the switch statement.
           */ 
          case 'd':				/* output decimal */
              d = va_arg(argp, signed int);
              if (d < 0) { negative = 1; u = -d; }  else { u = d; }
              base = 10;
              break;
          case 'u':				/* output unsigned long */
              u = va_arg(argp, unsigned long);
              base = 10;
              break;
          case 'x':				/* output hexadecimal */
              u = va_arg(argp, unsigned long);
              base = 0x10;
              break;
          case 's': 				/* output string */
              s = va_arg(argp, char *);
              if (s == NULL) s = "(null)";
              break;
          case '%':				/* output percent */
              s = "%";				 
              break;			

          /* Unrecognized key. */
          default:				/* echo back %key */
              s = "%?";				
              s[1] = c;				/* set unknown key */
          }

          /* Assume a number if no string is set. Convert to ascii. */
	  if (s == NULL) {
	    s = ascii + sizeof(ascii) - 1;
	    *s = 0;
	    do {
	      *--s = x2c[(u % base)];
	    }	/* work backwards */
	    while ((u /= base) > 0);
	  }

	  /* This is where the actual output for format "%key" is done. */
	  if (negative)
	    kputc(output_type, '-');	/* print sign if negative */
	  while (*s != 0) {
	    kputc(output_type, *s++);
	  }	/* print string/ number */
      } else {
	kputc(output_type, c);	/* print and continue */
      }
  }
  kputc(output_type, END_OF_KMESS);	/* terminate output */
  va_end(argp);					/* end variable arguments */
}

/*===========================================================================*
 *				kputc				     	     *
 *===========================================================================*/
PRIVATE void kputc(output_type, c)
int output_type;
int c;
{
	switch (output_type) {
	case KPUTC_TTY:
		kputc_tty(c);
		break;
	case KPUTC_XEN:
		kputc_xen(c);
		break;
	}
}

PRIVATE void kputc_xen(c)
int c;
{
	if (c != END_OF_KMESS)
		xen_debug_putc(c);
}

PRIVATE void kputc_tty(c)
int c;				/* character to append */
{
  /* Accumulate a single character for a kernel message. Send a notification
   * to the output driver if an END_OF_KMESS is encountered. 
   */
  if (c != END_OF_KMESS) {
    kmess.km_buf[kmess.km_next] = c;	/* put normal char in buffer */
    if (kmess.km_size < KMESS_BUF_SIZE)
      kmess.km_size += 1;
    kmess.km_next = (kmess.km_next + 1) % KMESS_BUF_SIZE;
  } else {
      int p, outprocs[] = OUTPUT_PROCS_ARRAY;
      for(p = 0; outprocs[p] != NONE; p++) {
         send_sig(outprocs[p], SIGKMESS);
      }
  }
}

