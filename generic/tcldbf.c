#include <string.h>
#include <tcl.h>
#include <shapefil.h>
#include "macros.h"
#include "dbfInt.h"
#include "handle.h"
#include "field.h"

CommandDecl (index);
CommandDecl (length);
CommandDecl (get);
CommandDecl (keys);
CommandDecl (size);
CommandDecl (values);
CommandDecl (foreach);

typedef struct
{
  int i, length, size;
} ForeachState;

static int ForeachAssignments (Tcl_Interp * const, ForeachState * const,
                               Tcl_Obj * const, DBFHandle const);
static int ForeachLoopStep (ClientData[], Tcl_Interp *, int);

static Tcl_Obj *NewAttributeObj (DBFHandle const, int const, int const);
static void SetAttributeObj (Tcl_Obj * const, DBFHandle const, int const,
                             int const);
static int ListObjAppendField (Tcl_Interp * const, Tcl_Obj * const,
                               DBFHandle const, int const);

Init (interp, dbfPtr)
{
  CreateCommand (interp, dbfPtr, index);
  CreateCommand (interp, dbfPtr, length);
  CreateCommand (interp, dbfPtr, get);
  CreateCommand (interp, dbfPtr, keys);
  CreateCommand (interp, dbfPtr, size);
  CreateCommand (interp, dbfPtr, values);
  CreateCommand (interp, dbfPtr, foreach);
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
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
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
      if (Tcl_ListObjAppendElement
          (interp, resultPtr, NewAttributeObj (dbfHandle, index, i)) != TCL_OK)
        {
          return TCL_ERROR;
        }
    }
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
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_SetIntObj (Tcl_GetObjResult (interp),
                 DBFGetRecordCount (handle->dbfHandle));
  return TCL_OK;
}

CommandDef (get, clientData, interp, objc, objv)
{
  Handle *handle;

  if (objc < 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename ?key ...?");
      return TCL_ERROR;
    }
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  switch (objc)
    {
    case 2:
      {
        DBFHandle dbfHandle;
        int fieldCount, i;
        Tcl_Obj *listPtr;
        Tcl_Obj *resultPtr;

        dbfHandle = handle->dbfHandle;
        fieldCount = DBFGetFieldCount (dbfHandle);
        resultPtr = Tcl_GetObjResult (interp);
        for (i = 0; i < fieldCount; i++)
          {
            if (Tcl_ListObjAppendElement
                (interp, resultPtr, Dbf_NewFieldObj (dbfHandle, i)) != TCL_OK)
              {
                return TCL_ERROR;
              }
            listPtr = Tcl_NewObj ();
            if (ListObjAppendField (interp, listPtr, dbfHandle, i) != TCL_OK)
              {
                return TCL_ERROR;
              }
            if (Tcl_ListObjAppendElement (interp, resultPtr, listPtr) != TCL_OK)
              {
                return TCL_ERROR;
              }
          }
      }
      break;
    case 3:
      {
        DBFHandle dbfHandle;
        int index;
        Tcl_Obj *resultPtr;

        dbfHandle = handle->dbfHandle;
        if (DbfGetFieldIndexFromObj (interp, dbfHandle, objv[2], &index) !=
            TCL_OK)
          {
            return TCL_ERROR;
          }
        resultPtr = Tcl_GetObjResult (interp);
        if (ListObjAppendField (interp, resultPtr, dbfHandle, index) != TCL_OK)
          {
            return TCL_ERROR;
          }
      }
      break;
    case 4:
      {
        DBFHandle dbfHandle;
        int fieldIndex, recordIndex;
        Tcl_Obj *resultPtr;

        dbfHandle = handle->dbfHandle;
        if (DbfGetFieldIndexFromObj (interp, dbfHandle, objv[2], &fieldIndex) !=
            TCL_OK)
          {
            return TCL_ERROR;
          }
        if (Tcl_GetIntFromObj (interp, objv[3], &recordIndex) != TCL_OK)
          {
            return TCL_ERROR;
          }
        resultPtr = Tcl_GetObjResult (interp);
        SetAttributeObj (resultPtr, dbfHandle, recordIndex, fieldIndex);
        break;
      }
    default:
      Tcl_SetResult (interp, "missing value to go with key", TCL_STATIC);
      Tcl_SetErrorCode (interp, "DBF", "VALUE", "DICTIONARY", NULL);
      return TCL_ERROR;
    }
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
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  dbfHandle = handle->dbfHandle;
  fieldCount = DBFGetFieldCount (dbfHandle);
  resultPtr = Tcl_GetObjResult (interp);
  for (i = 0; i < fieldCount; i++)
    {
      if (Tcl_ListObjAppendElement
          (interp, resultPtr, Dbf_NewFieldObj (dbfHandle, i)) != TCL_OK)
        {
          return TCL_ERROR;
        }
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
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  Tcl_SetIntObj (Tcl_GetObjResult (interp),
                 DBFGetFieldCount (handle->dbfHandle));
  return TCL_OK;
}

CommandDef (values, clientData, interp, objc, objv)
{
  Handle *handle;
  DBFHandle dbfHandle;
  int fieldCount, i;
  Tcl_Obj *resultPtr, *listPtr;

  if (objc != 2)
    {
      Tcl_WrongNumArgs (interp, 1, objv, "filename");
      return TCL_ERROR;
    }
  if (DbfGetHandleFromObj (interp, objv[1], &handle) != TCL_OK)
    {
      return TCL_ERROR;
    }
  dbfHandle = handle->dbfHandle;
  fieldCount = DBFGetFieldCount (dbfHandle);
  resultPtr = Tcl_GetObjResult (interp);
  for (i = 0; i < fieldCount; i++)
    {
      listPtr = Tcl_NewObj ();
      if (ListObjAppendField (interp, listPtr, dbfHandle, i) != TCL_OK)
        {
          return TCL_ERROR;
        }
      if (Tcl_ListObjAppendElement (interp, resultPtr, listPtr) != TCL_OK)
        {
          return TCL_ERROR;
        }
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
      if (Tcl_ListObjAppendElement
          (interp, valuePtr, NewAttributeObj (dbfHandle, i, j)) != TCL_OK)
        {
          return TCL_ERROR;
        }
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
           ForeachAssignments (interp, statePtr, varPtr, dbfHandle)) != TCL_OK)
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

void
AppendObj (Tcl_DString * const dsPtr, Tcl_Obj * const objPtr)
{
  int length;
  char const *bytes = Tcl_GetStringFromObj (objPtr, &length);

  Tcl_DStringAppend (dsPtr, bytes, length);
}

static Tcl_Obj *
NewAttributeObj (DBFHandle const dbfHandle, int const record, int const field)
{
  if (DBFIsAttributeNULL (dbfHandle, record, field))
    {
      return Tcl_NewObj ();
    }
  switch (DBFGetFieldInfo (dbfHandle, field, NULL, NULL, NULL))
    {
    case FTInteger:
      return Tcl_NewIntObj (DBFReadIntegerAttribute (dbfHandle, record, field));
    case FTDouble:
      return
        Tcl_NewDoubleObj (DBFReadDoubleAttribute (dbfHandle, record, field));
    default:
      return
        Tcl_NewStringObj (DBFReadStringAttribute (dbfHandle, record, field),
                          -1);
    }
}

static void
SetAttributeObj (Tcl_Obj * const objPtr, DBFHandle const dbfHandle,
                 int const record, int const field)
{
  if (DBFIsAttributeNULL (dbfHandle, record, field))
    {
      return;
    }
  switch (DBFGetFieldInfo (dbfHandle, field, NULL, NULL, NULL))
    {
    case FTInteger:
      Tcl_SetIntObj (objPtr,
                     DBFReadIntegerAttribute (dbfHandle, record, field));
      break;
    case FTDouble:
      Tcl_SetDoubleObj (objPtr,
                        DBFReadDoubleAttribute (dbfHandle, record, field));
      break;
    default:
      Tcl_SetStringObj (objPtr,
                        DBFReadStringAttribute (dbfHandle, record, field), -1);
    }
}

static int
ListObjAppendField (Tcl_Interp * const interp, Tcl_Obj * const listPtr,
                    DBFHandle const dbfHandle, int const field)
{
  int const recordCount = DBFGetRecordCount (dbfHandle);
  int i;

  for (i = 0; i < recordCount; i++)
    {
      if (Tcl_ListObjAppendElement
          (interp, listPtr, NewAttributeObj (dbfHandle, i, field)) != TCL_OK)
        {
          return TCL_ERROR;
        }
    }
  return TCL_OK;
}
