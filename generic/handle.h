#ifndef HANDLE_H__
#define HANDLE_H__

extern Tcl_ObjType const dbfHandleType;

typedef struct
{
  int refCount;
  DBFHandle dbfHandle;
  int length;
  char bytes[FLEX_ARRAY];
} Handle;

extern int Dbf_GetHandleFromObj (Tcl_Interp *, Tcl_Obj *, Handle **);

#define DbfGetHandleFromObj(interp, objPtr, handlePtr) \
  (((objPtr)->typePtr == &dbfHandleType) \
    ? ((*(handlePtr) = GetHandle(objPtr)), TCL_OK) \
    : Dbf_GetHandleFromObj ((interp), (objPtr), (handlePtr)))

#define GetHandle(objPtr) ((Handle *) (objPtr)->internalRep.otherValuePtr)

#endif
