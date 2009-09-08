#include <string.h>
#include <tcl.h>
#include <shapefil.h>
#include "dbfInt.h"
#include "field.h"

static void DupFieldInternalRep (Tcl_Obj *, Tcl_Obj *);

Tcl_ObjType const dbfFieldType = {
  "dbfField",
  NULL,
  DupFieldInternalRep,
  NULL,
  NULL
};

#define SetFieldIndex(objPtr, index) do { \
  (objPtr)->internalRep.ptrAndLongRep.value = (index); \
} while (0)

#define SetDBFHandle(objPtr, dbfHandle) do { \
  (objPtr)->internalRep.ptrAndLongRep.ptr = (dbfHandle); \
} while (0)

extern Tcl_Obj *
Dbf_NewFieldObj (DBFHandle const dbfHandle, int const field)
{
  char fieldName[12];
  int length;
  Tcl_Obj *fieldPtr;

  DBFGetFieldInfo (dbfHandle, field, fieldName, 0, 0);
  length = strlen (fieldName);
  fieldPtr = Tcl_NewObj ();
  fieldPtr->typePtr = &dbfFieldType;
  fieldPtr->bytes = ckalloc (length + 1);
  memcpy (fieldPtr->bytes, fieldName, length + 1);
  fieldPtr->length = length;
  SetDBFHandle (fieldPtr, dbfHandle);
  SetFieldIndex (fieldPtr, field);
  return fieldPtr;
}

extern int
Dbf_GetFieldIndexFromObj (Tcl_Interp * interp, DBFHandle dbfHandle,
                          Tcl_Obj * objPtr, int *indexPtr)
{
  if (objPtr->typePtr == &dbfFieldType && GetDBFHandle (objPtr) == dbfHandle)
    {  
      *indexPtr = GetFieldIndex (objPtr);
      return TCL_OK;
    }
  else
    {
      if ((*indexPtr =
           DBFGetFieldIndex (dbfHandle, Tcl_GetString (objPtr))) == -1)
        {
          Tcl_SetResult (interp, "missing value to go with key", TCL_STATIC);
          Tcl_SetErrorCode (interp, "DBF", "VALUE", "DICTIONARY", NULL);
          return TCL_ERROR;
        }
      FreeInternalRep (objPtr);
      objPtr->typePtr = &dbfFieldType;
      SetFieldIndex (objPtr, *indexPtr);
      SetDBFHandle (objPtr, dbfHandle);
      return TCL_OK;
    }
}

static void
DupFieldInternalRep (Tcl_Obj * srcPtr, Tcl_Obj * dupPtr)
{
  int length;

  dupPtr->typePtr = &dbfFieldType;
  SetFieldIndex (dupPtr, GetFieldIndex (srcPtr));
  SetDBFHandle (dupPtr, GetDBFHandle (srcPtr));
  dupPtr->bytes = ckalloc ((length = srcPtr->length) + 1);
  memcpy (dupPtr->bytes, srcPtr->bytes, length + 1);
  dupPtr->length = length;
}
