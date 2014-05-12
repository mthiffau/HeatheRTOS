#ifndef XARG_H
#define XARG_H

/* Typedef for the argument list */
typedef char *va_list;

#define __va_argsiz(t) \
    (((sizeof(t) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

/* Start getting the arugment list */
#define va_start(ap, pN) \
    ((ap) = ((va_list) __builtin_next_arg(pN)))

/* Finish getting the argument list */
#define va_end(ap) \
    ((void)0)

/* Get an argument from the argument list */
#define va_arg(ap, t) \
    (((ap) = (ap) + __va_argsiz(t)), *((t*) (void*) ((ap) - __va_argsiz(t))))

#endif
