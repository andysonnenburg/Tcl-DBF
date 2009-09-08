#include <string.h>
#include <tcl.h>
#include <shapefil.h>
#include "dbfInt.h"
#include "handle.h"

static void FreeHandleInternalRep (Tcl_Obj *);
static void DupHandleInternalRep (Tcl_Obj *, Tcl_Obj *);
static void UpdateHandleString (Tcl_Obj *);
static int SetHandleFromAny (Tcl_Interp *, Tcl_Obj *);

Tcl_ObjType const dbfHandleType = {
  "dbfHandle",
  FreeHandleInternalRep,
  DupHandleInternalRep,
  UpdateHandleString,
  SetHandleFromAny
};

#define SetHandle(objPtr, handle) do { \
  (objPtr)->internalRep.otherValuePtr = (handle); \
} while (0)

static DBFHandle DbfOpen (Tcl_Interp * const, Tcl_Obj * const);

#define Preserve(handle) do { handle->refCount++; } while (0)

static void Release (Handle *);

extern int
Dbf_GetHandleFromObj (Tcl_Interp * interp, Tcl_Obj * objPtr,
                      Handle ** handlePtr)
{
  if (objPtr->typePtr == &dbfHandleType)
    {
      *handlePtr = GetHandle (objPtr);
      return TCL_OK;
    }
  else
    {
      int length;
      char const *bytes = Tcl_GetStringFromObj (objPtr, &length);
      DBFHandle dbfHandle;
      Handle *handle;

      if ((dbfHandle = DbfOpen (interp, objPtr)) == NULL)
        {
          return TCL_ERROR;
        }

      handle =
        (Handle *) ckalloc (sizeof (*handle) +
                            sizeof (char) * (length + 1 + FLEX_ARRAY_SIZE));
      handle->refCount = 0;
      handle->dbfHandle = dbfHandle;
      memcpy (handle->bytes, bytes, length + 1);
      handle->length = length;

      FreeInternalRep (objPtr);
      objPtr->typePtr = &dbfHandleType;
      SetHandle (objPtr, handle);
      Preserve (handle);
      *handlePtr = handle;
      return TCL_OK;
    }
}

static void
FreeHandleInternalRep (Tcl_Obj * objPtr)
{
  Release (GetHandle (objPtr));
}

static void
DupHandleInternalRep (Tcl_Obj * srcPtr, Tcl_Obj * dupPtr)
{
  Handle *handle;

  handle = GetHandle (srcPtr);
  dupPtr->typePtr = &dbfHandleType;
  SetHandle (dupPtr, handle);
  Preserve (handle);
}

static void
UpdateHandleString (Tcl_Obj * objPtr)
{
  Handle *handle;
  int length;

  objPtr->length = length = (handle = GetHandle (objPtr))->length;
  objPtr->bytes = ckalloc (sizeof (char) * (length + 1));
  memcpy (objPtr->bytes, handle->bytes, length + 1);
}

static int
SetHandleFromAny (Tcl_Interp * interp, Tcl_Obj * objPtr)
{
  Handle *handle;
  return Dbf_GetHandleFromObj (interp, objPtr, &handle);
}


static DBFHandle
DbfOpen (Tcl_Interp * const interp, Tcl_Obj * const pathPtr)
{
  DBFHandle dbfHandle;

  if ((dbfHandle = DBFOpen (Tcl_GetString (pathPtr), "rb")) == NULL)
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
      return NULL;
    }
  return dbfHandle;
}

static void
Release (Handle * handle)
{
  if (--handle->refCount <= 0)
    {
      DBFClose (handle->dbfHandle);
      ckfree ((char *) handle);
    }
}
