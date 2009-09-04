#include <string.h>
#include <tcl.h>
#include <shapefil.h>
#include "macros.h"

CommandDecl (length);
CommandDecl (index);
CommandDecl (size);
CommandDecl (keys);
CommandDecl (foreach);

typedef struct
{
  int i, length, size;
} ForeachState;

static int ForeachAssignments (Tcl_Interp * const, ForeachState * const,
                               Tcl_Obj * const, DBFHandle const);
static int ForeachLoopStep (ClientData[], Tcl_Interp *, int);

static void FreeHandleInternalRep (Tcl_Obj *);
static void DupHandleInternalRep (Tcl_Obj *, Tcl_Obj *);
static void UpdateHandleString (Tcl_Obj *);
static int SetHandleFromAny (Tcl_Interp *, Tcl_Obj *);

static Tcl_ObjType dbfType = {
  "dbfHandle",
  FreeHandleInternalRep,
  DupHandleInternalRep,
  UpdateHandleString,
  SetHandleFromAny
};

typedef struct
{
  int refCount;
  DBFHandle dbfHandle;
  int length;
  char bytes[FLEX_ARRAY];
} Handle;

#define GetHandle(objPtr) ((Handle *) (objPtr)->internalRep.otherValuePtr)

#define DbfGetHandleFromObj(interp, objPtr, handlePtr) \
  (((objPtr)->typePtr == &dbfType) \
    ? ((*(handlePtr) = GetHandle(objPtr)), TCL_OK) \
    : Dbf_GetHandleFromObj ((interp), (objPtr), (handlePtr)))

static int Dbf_GetHandleFromObj (Tcl_Interp *, Tcl_Obj *, Handle **);

#define SetHandle(objPtr, handle) do { \
  (objPtr)->internalRep.otherValuePtr = (handle); \
} while (0)

static Tcl_Obj *NewAttributeObj (DBFHandle, int, int);
static DBFHandle DbfOpen (Tcl_Interp *, Tcl_Obj *);

#define Preserve(handle) do { handle->refCount++; } while (0)
static void Release (Handle *);

#define FreeInternalRep(objPtr) do { \
  if ((objPtr)->typePtr != NULL && \
      (objPtr)->typePtr->freeIntRepProc != NULL) { \
    (objPtr)->typePtr->freeIntRepProc(objPtr); \
  } \
} while (0)

#define AppendLiteral(dsPtr, literal) \
  Tcl_DStringAppend((dsPtr), literal "", sizeof literal - 1)
static void AppendObj (Tcl_DString * const, Tcl_Obj * const);

Init (Dbf, interp, dbfPtr)
{
  CreateCommand (interp, dbfPtr, length);
  CreateCommand (interp, dbfPtr, index);
  CreateCommand (interp, dbfPtr, size);
  CreateCommand (interp, dbfPtr, keys);
  CreateCommand (interp, dbfPtr, foreach);
  return TCL_OK;
}

CommandDef (length, clientData, interp, objc, objv)
{
  Handle *handle;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (Dbf_GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_SetIntObj (Tcl_GetObjResult (interp),
                 DBFGetRecordCount (handle->dbfHandle));
  return TCL_OK;
}

CommandDef (index, clientData, interp, objc, objv)
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
  if (Dbf_GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
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
      Tcl_ListObjAppendElement (interp, resultPtr,
                                NewAttributeObj (dbfHandle, index, i));
    }
  return TCL_OK;
}

CommandDef (size, clientData, interp, objc, objv)
{
  Handle *handle;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (Dbf_GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_SetIntObj (Tcl_GetObjResult (interp),
                 DBFGetFieldCount (handle->dbfHandle));
  return TCL_OK;
}

CommandDef (keys, clientData, interp, objc, objv)
{
  Handle *handle;
  DBFHandle dbfHandle;
  int fieldCount, i;
  Tcl_Obj *resultPtr;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (Dbf_GetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  dbfHandle = handle->dbfHandle;
  fieldCount = DBFGetFieldCount (dbfHandle);
  resultPtr = Tcl_GetObjResult (interp);
  for (i = 0; i < fieldCount; i++)
    {
      char fieldName[12];
      DBFGetFieldInfo (dbfHandle, i, fieldName, 0, 0);
      Tcl_ListObjAppendElement (interp, resultPtr,
                                Tcl_NewStringObj (fieldName, -1));
    }
  return TCL_OK;
}

CommandDef (foreach, clientData, interp, objc, objv)
{
  Handle *handle;
  DBFHandle dbfHandle;
  int recordCount, fieldCount;
  ForeachState *statePtr;

  if (objc != 4)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "var filename command");
      return TCL_ERROR;
    }
  if (DbfGetHandleFromObj (interp, objv[2], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  if ((recordCount = DBFGetRecordCount (dbfHandle = handle->dbfHandle)) == 0)
    {
      return TCL_OK;
    }
  if ((fieldCount = DBFGetFieldCount (dbfHandle)) == 0)
    {
      return TCL_OK;
    }
  statePtr = (ForeachState *) ckalloc (sizeof (*statePtr));
  statePtr->i = 0;
  statePtr->length = recordCount;
  statePtr->size = fieldCount;
  if (ForeachAssignments (interp, statePtr, objv[1], dbfHandle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_NRAddCallback (interp, ForeachLoopStep, statePtr, objv[1], dbfHandle,
                     objv[3]);
  return Tcl_NREvalObj (interp, objv[3], 0);
}

static int
ForeachAssignments (Tcl_Interp * const interp, ForeachState * const statePtr,
                    Tcl_Obj * const varPtr, DBFHandle const dbfHandle)
{
  Tcl_Obj *const valuePtr = Tcl_NewObj ();
  int const i = statePtr->i;
  int j;
  int const size = statePtr->size;

  for (j = 0; j < size; j++)
    {
      Tcl_ListObjAppendElement (interp, valuePtr,
                                NewAttributeObj (dbfHandle, i, j));
    }
  if (Tcl_ObjSetVar2 (interp, varPtr, NULL, valuePtr, TCL_LEAVE_ERR_MSG) ==
      NULL)
    {
      return TCL_ERROR;
    }
  return TCL_OK;
}

static int
ForeachLoopStep (ClientData data[], Tcl_Interp * interp, int result)
{
  ForeachState *const statePtr = data[0];
  Tcl_Obj *const varPtr = data[1];
  DBFHandle const dbfHandle = data[2];
  Tcl_Obj *const bodyPtr = data[3];

  switch (result)
    {
    case TCL_CONTINUE:
      result = TCL_OK;
    case TCL_OK:
      break;
    case TCL_BREAK:
      result = TCL_OK;
      goto done;
    case TCL_ERROR:
    default:
      goto done;
    }

  if (statePtr->length > ++statePtr->i)
    {
      if ((result =
           ForeachAssignments (interp, statePtr, varPtr,
                               dbfHandle)) != TCL_OK)
        {
          goto done;
        }
      Tcl_NRAddCallback (interp, ForeachLoopStep, statePtr, varPtr, dbfHandle,
                         bodyPtr);
      return Tcl_NREvalObj (interp, bodyPtr, 0);
    }
  Tcl_ResetResult (interp);
done:
  ckfree ((char *) statePtr);
  return result;
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
  dupPtr->typePtr = &dbfType;
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

static int
Dbf_GetHandleFromObj (Tcl_Interp * interp, Tcl_Obj * objPtr,
                      Handle ** handlePtr)
{
  Handle *handle;

  if (objPtr->typePtr == &dbfType)
    {
      handle = GetHandle (objPtr);
    }
  else
    {
      int length;
      char const *bytes = Tcl_GetStringFromObj (objPtr, &length);
      DBFHandle dbfHandle;

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
      objPtr->typePtr = &dbfType;
      SetHandle (objPtr, handle);
      Preserve (handle);
    }
  *handlePtr = handle;
  return TCL_OK;
}

static Tcl_Obj *
NewAttributeObj (DBFHandle dbfHandle, int index, int field)
{
  if (DBFIsAttributeNULL (dbfHandle, index, field))
    {
      return Tcl_NewObj ();
    }
  switch (DBFGetFieldInfo (dbfHandle, field, NULL, NULL, NULL))
    {
    case FTInteger:
      return
        Tcl_NewIntObj (DBFReadIntegerAttribute (dbfHandle, index, field));
    case FTDouble:
      return
        Tcl_NewDoubleObj (DBFReadDoubleAttribute (dbfHandle, index, field));
    default:
      return
        Tcl_NewStringObj (DBFReadStringAttribute (dbfHandle, index, field),
                          -1);
    }
}

static DBFHandle
DbfOpen (Tcl_Interp * interp, Tcl_Obj * pathPtr)
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

static void
AppendObj (Tcl_DString * const dsPtr, Tcl_Obj * const objPtr)
{
  int length;
  char const *bytes = Tcl_GetStringFromObj (objPtr, &length);

  Tcl_DStringAppend (dsPtr, bytes, length);
}
