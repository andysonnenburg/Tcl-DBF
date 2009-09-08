#ifndef DBF_INT_H__
#define DBF_INT_H__

#ifndef FLEX_ARRAY
/*
 * See if our compiler is known to support flexible array members.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) && (!defined(__SUNPRO_C) || (__SUNPRO_C > 0x580))
# define FLEX_ARRAY             /* empty */
#elif defined(__GNUC__)
# if (__GNUC__ >= 3)
#  define FLEX_ARRAY            /* empty */
# else
#  define FLEX_ARRAY 0          /* older GNU extension */
# endif
#endif

/*
 * Otherwise, default to safer but a bit wasteful traditional style
 */
#ifndef FLEX_ARRAY
# define FLEX_ARRAY 1
#endif
#endif

#define FLEX_ARRAY_SIZE (FLEX_ARRAY + 0)

#define FreeInternalRep(objPtr) do { \
  if ((objPtr)->typePtr != NULL && \
      (objPtr)->typePtr->freeIntRepProc != NULL) { \
    (objPtr)->typePtr->freeIntRepProc(objPtr); \
  } \
} while (0)

#define AppendLiteral(dsPtr, literal) \
  Tcl_DStringAppend((dsPtr), literal "", sizeof literal - 1)

void AppendObj (Tcl_DString * const, Tcl_Obj * const);

#endif
