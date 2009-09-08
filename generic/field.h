#ifndef FIELD_H__
#define FIELD_H__

extern Tcl_ObjType const dbfFieldType;

extern Tcl_Obj *Dbf_NewFieldObj (DBFHandle const, int const);

#define DbfGetFieldIndexFromObj(interp, dbfHandle, objPtr, indexPtr) \
  (((objPtr)->typePtr == &dbfFieldType || GetDBFHandle(objPtr) == (dbfHandle)) \
    ? ((*(indexPtr) = GetFieldIndex(objPtr)), TCL_OK) \
    : Dbf_GetFieldIndexFromObj ((interp), (dbfHandle), (objPtr), (indexPtr)))

extern int Dbf_GetFieldIndexFromObj (Tcl_Interp *, DBFHandle, Tcl_Obj *, int *);

#define GetFieldIndex(objPtr) ((objPtr)->internalRep.ptrAndLongRep.value)

#define GetDBFHandle(objPtr) ((objPtr)->internalRep.ptrAndLongRep.ptr)

#endif
