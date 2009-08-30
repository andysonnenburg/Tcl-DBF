#include <stdlib.h>
#include <string.h>
#include <tcl.h>
#include <shapefil.h>

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

extern int Dbf_Init (Tcl_Interp *);

#define DbfCommandDecl(NAME) \
static int Dbf##NAME (ClientData, Tcl_Interp *, int, Tcl_Obj * const *); \
static int DbfNR##NAME (ClientData, Tcl_Interp *, int, Tcl_Obj * const *); \
\
static int \
Dbf##NAME (ClientData clientData, Tcl_Interp * interp, int objc, \
         Tcl_Obj * const * objv) \
{ \
  return Tcl_NRCallObjProc (interp, DbfNR##NAME, clientData, objc, objv); \
}

#define DbfInit(INTERP, dbfPtr) do { \
  if (Tcl_InitStubs ((INTERP), TCL_VERSION, 0) == NULL) \
    { \
      return TCL_ERROR; \
    } \
  dbfPtr = Tcl_CreateNamespace((INTERP), "::dbf", NULL, NULL); \
} while (0)

#define DbfProvide(INTERP, dbfPtr) \
  Tcl_CreateEnsemble((INTERP), "::dbf", dbfPtr, TCL_ENSEMBLE_PREFIX); \
  if (Tcl_PkgProvide (INTERP, "dbf", PACKAGE_VERSION) != TCL_OK) \
    { \
      return TCL_ERROR; \
    };

#define DbfCommandDef(NAME, CLIENT_DATA, INTERP, OBJC, OBJV) \
static int \
DbfNR##NAME (ClientData CLIENT_DATA, Tcl_Interp * INTERP, int OBJC, \
           Tcl_Obj * const OBJV[])

#define DbfCreateCommand(INTERP, dbfPtr, NAME) do { \
  Tcl_NRCreateCommand((INTERP), "::dbf::" #NAME, Dbf##NAME, DbfNR##NAME, NULL, NULL); \
  if (Tcl_Export (INTERP, dbfPtr, #NAME, 0) != TCL_OK) { \
    return TCL_ERROR; \
  } \
} while (0)


/* *INDENT-OFF* */

DbfCommandDecl (open)
DbfCommandDecl (length)
DbfCommandDecl (index)
DbfCommandDecl (keys)
DbfCommandDecl (close) 
DbfCommandDecl (foreach)

/* *INDENT-ON* */

static void FreeDbfInternalRep (Tcl_Obj *);
static void DupDbfInternalRep (Tcl_Obj *, Tcl_Obj *);
static void UpdateDbfString (Tcl_Obj *);
static int SetDbfFromAny (Tcl_Interp *, Tcl_Obj *);
static int SetDbfFromPath (Tcl_Interp *, Tcl_Obj *, Tcl_Obj *);

static Tcl_ObjType dbfType = {
  "dbfHandle",
  FreeDbfInternalRep,
  DupDbfInternalRep,
  UpdateDbfString,
  SetDbfFromAny
};

typedef struct
{
  int refCount;
  DBFHandle dbfHandle;
  int length;
  char bytes[FLEX_ARRAY];
} Handle;

#define GetHandle(objPtr) ((Handle *) (objPtr)->internalRep.otherValuePtr)
static int GetHandleFromObj (Tcl_Interp *, Tcl_Obj *, Handle **);
static void SetHandleObj (Tcl_Obj *, Handle *);

#define Preserve(handle) do { handle->refCount++; } while (0)
static void Release (Handle *);

#define AppendLiteral(dsPtr, literal) \
  Tcl_DStringAppend((dsPtr), literal "", sizeof literal - 1)
static void AppendObj (Tcl_DString *, Tcl_Obj *);

int
Dbf_Init (Tcl_Interp * interp)
{
  Tcl_Namespace *dbfPtr;

  DbfInit (interp, dbfPtr);
  DbfCreateCommand (interp, dbfPtr, open);
  DbfCreateCommand (interp, dbfPtr, length);
  DbfCreateCommand (interp, dbfPtr, index);
  DbfCreateCommand (interp, dbfPtr, keys);
  DbfCreateCommand (interp, dbfPtr, close);
  DbfCreateCommand (interp, dbfPtr, foreach);
  DbfProvide (interp, dbfPtr);
  return TCL_OK;
}

DbfCommandDef (open, clientData, interp, objc, objv)
{
  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (SetDbfFromPath (interp, Tcl_GetObjResult (interp), objv[1]) != TCL_OK)
    {
      return TCL_ERROR;
    }
  return TCL_OK;
}

DbfCommandDef (length, clientData, interp, objc, objv)
{
  Handle *handle;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_SetIntObj (Tcl_GetObjResult (interp),
                 DBFGetRecordCount (handle->dbfHandle));
  return TCL_OK;
}

DbfCommandDef (index, clientData, interp, objc, objv)
{
  Handle *handle;
  int index, fieldCount, i;
  DBFHandle dbfHandle;
  Tcl_Obj *resultPtr;

  if (objc != 3)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename index");
      return TCL_ERROR;
    }
  if (GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  if (Tcl_GetIntFromObj (interp, objv[2], &index) != TCL_OK)
    {
      return TCL_ERROR;
    }
  dbfHandle = handle->dbfHandle;
  fieldCount = DBFGetFieldCount (dbfHandle);
  resultPtr = Tcl_GetObjResult (interp);
  for (i = 0; i < fieldCount; i++)
    {
      switch (DBFGetFieldInfo (dbfHandle, i, NULL, NULL, NULL))
        {
        case FTInteger:
          Tcl_ListObjAppendElement (interp, resultPtr,
                                    Tcl_NewIntObj (DBFReadIntegerAttribute
                                                   (dbfHandle, index, i)));
          break;
        case FTDouble:
          Tcl_ListObjAppendElement (interp, resultPtr,
                                    Tcl_NewDoubleObj (DBFReadDoubleAttribute
                                                      (dbfHandle, index, i)));
          break;
        default:
          Tcl_ListObjAppendElement (interp, resultPtr,
                                    Tcl_NewStringObj (DBFReadStringAttribute
                                                      (dbfHandle, index, i),
                                                      -1));
        }
    }
  return TCL_OK;
}

DbfCommandDef (keys, clientData, interp, objc, objv) {
  return TCL_OK;
}

DbfCommandDef (close, clientData, interp, objc, objv)
{
  Handle *handle;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  DBFClose (handle->dbfHandle);
  handle->dbfHandle = NULL;
  return TCL_OK;
}

DbfCommandDef (foreach, clientData, interp, objc, objv)
{
  return TCL_OK;
}

static void
FreeDbfInternalRep (Tcl_Obj * objPtr)
{
  Release (GetHandle (objPtr));
}

static void
DupDbfInternalRep (Tcl_Obj * srcPtr, Tcl_Obj * dupPtr)
{
  SetHandleObj (dupPtr, GetHandle (srcPtr));
}

static void
UpdateDbfString (Tcl_Obj * objPtr)
{
  Handle *handle;
  int length;

  length = (handle = GetHandle (objPtr))->length;
  objPtr->bytes = ckalloc (sizeof (char) * (length + 1));
  memcpy (objPtr->bytes, handle->bytes, length + 1);
  objPtr->length = length;
}

static int
SetDbfFromAny (Tcl_Interp * interp, Tcl_Obj * objPtr)
{
  return SetDbfFromPath (interp, objPtr, objPtr);
}

static int
GetHandleFromObj (Tcl_Interp * interp, Tcl_Obj * objPtr, Handle ** handlePtr)
{
  if (objPtr->typePtr != &dbfType && SetDbfFromAny (interp, objPtr) != TCL_OK)
    {
      return TCL_ERROR;
    }
  *handlePtr = GetHandle (objPtr);
  return TCL_OK;
}

static void
SetHandleObj (Tcl_Obj * objPtr, Handle * handle)
{
  Tcl_InvalidateStringRep (objPtr);
  Preserve (handle);
  objPtr->internalRep.otherValuePtr = handle;
  objPtr->typePtr = &dbfType;
}

static int
SetDbfFromPath (Tcl_Interp * interp, Tcl_Obj * objPtr, Tcl_Obj * pathPtr)
{
  Tcl_Obj *normalizedPtr;
  DBFHandle dbfHandle;
  Handle *handle;

  if ((normalizedPtr = Tcl_FSGetNormalizedPath (interp, pathPtr)) == NULL)
    {
      return TCL_ERROR;
    }
  {
    int length;
    char const *filename = Tcl_GetStringFromObj (normalizedPtr, &length);
    dbfHandle = DBFOpen (filename, "rb");
    if (dbfHandle == NULL)
      {
        if (interp != NULL)
          {
            Tcl_DString message;

            Tcl_DStringInit (&message);
            AppendLiteral (&message, "couldn't open \"");
            AppendObj (&message, pathPtr);
            AppendLiteral (&message, "\"");
            Tcl_DStringResult (interp, &message);
            Tcl_DStringFree (&message);
          }
        return TCL_ERROR;
      }
    handle = malloc (sizeof (*handle) + sizeof (char) * (length
#if FLEX_ARRAY + 0 == 0
                                                         + 1
#else
                                                         - FLEX_ARRAY + 1
#endif
                     ));
    handle->refCount = 0;
    memcpy (handle->bytes, filename, length + 1);
    handle->length = length;
  }
  handle->dbfHandle = dbfHandle;
  SetHandleObj (objPtr, handle);
  return TCL_OK;
}

static void
Release (Handle * handle)
{
  if (--handle->refCount <= 0)
    {
      if (handle->dbfHandle != NULL)
        {
          DBFClose (handle->dbfHandle);
        }
      puts ("hi");
      free (handle);
      puts ("hello");
    }
}

static void
AppendObj (Tcl_DString * dsPtr, Tcl_Obj * objPtr)
{
  int length;
  char const *bytes = Tcl_GetStringFromObj (objPtr, &length);

  Tcl_DStringAppend (dsPtr, bytes, length);
}
