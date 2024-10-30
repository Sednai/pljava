/*
 * Copyright (c) 2004-2020 Tada AB and other contributors, as listed below.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the The BSD 3-Clause License
 * which accompanies this distribution, and is available at
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Contributors:
 *   Tada AB
 *   PostgreSQL Global Development Group
 *   Chapman Flack
 */
#include "pljava/type/Type_priv.h"
#include "pljava/type/Array.h"
#include "pljava/Invocation.h"
#include <math.h>

static TypeClass s_longClass;
static jclass    s_Long_class;
static jmethodID s_Long_init;
static jmethodID s_Long_longValue;

/*
 * long primitive type.
 */
static Datum _asDatum(jlong v)
{
	MemoryContext currCtx = Invocation_switchToUpperContext();
	Datum ret = Int64GetDatum(v);
	MemoryContextSwitchTo(currCtx);
	return ret;
}

static Datum _long_invoke(Type self, Function fn, PG_FUNCTION_ARGS)
{
	return _asDatum(pljava_Function_longInvoke(fn));
}

static jvalue _long_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.j = DatumGetInt64(arg);
	return result;
}

static jvalue _longArray_coerceDatum(Type self, Datum arg)
{
	jvalue     result;
	ArrayType* v      = DatumGetArrayTypeP(arg);
	
	if (ARR_NDIM(v) != 2 ) {
		jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
		jlongArray longArray = JNI_newLongArray(nElems);

		if(ARR_HASNULL(v))
		{
			jsize idx;
			jboolean isCopy = JNI_FALSE;
			bits8* nullBitMap = ARR_NULLBITMAP(v);
			jlong* values = (jlong*)ARR_DATA_PTR(v);
			jlong* elems  = JNI_getLongArrayElements(longArray, &isCopy);
			for(idx = 0; idx < nElems; ++idx)
			{
				if(arrayIsNull(nullBitMap, idx))
					elems[idx] = 0;
				else
					elems[idx] = *values++;
			}
			JNI_releaseLongArrayElements(longArray, elems, JNI_COMMIT);
		}
		else
			JNI_setLongArrayRegion(longArray, 0, nElems, (jlong*)ARR_DATA_PTR(v));
		
		result.l = (jobject)longArray;
		return result;
	
	} else {
		// Create outer array
		jobjectArray objArray = JNI_newObjectArray(ARR_DIMS(v)[0], JNI_newGlobalRef(PgObject_getJavaClass("[J")), 0);
		
		int nc = 0;
		int NaNc = 0;

		if(ARR_HASNULL(v)) {
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jlongArray innerArray = JNI_newLongArray(ARR_DIMS(v)[1]);
							
				jboolean isCopy = JNI_FALSE;
				bits8* nullBitMap = ARR_NULLBITMAP(v);

				jlong* elems  = JNI_getLongArrayElements(innerArray, &isCopy);
			
				for(int jdx = 0; jdx < ARR_DIMS(v)[1]; ++jdx) {
					if(arrayIsNull(nullBitMap, nc)) {
						elems[jdx] = 0;
						NaNc++;
					}
					else {
						elems[jdx] = *((jlong*) (ARR_DATA_PTR(v)+(nc-NaNc)*sizeof(long)));
					}
					nc++;
				}
				JNI_releaseLongArrayElements(innerArray, elems, JNI_COMMIT);
	
				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);
			}	
		} else {
				
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jlongArray innerArray = JNI_newLongArray(ARR_DIMS(v)[1]);
				
				JNI_setLongArrayRegion(innerArray, 0, ARR_DIMS(v)[1], (jlong *) (ARR_DATA_PTR(v) + nc*sizeof(long) ));
				nc += ARR_DIMS(v)[1];

				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);		
			}
		}

		result.l = (jobject) objArray;		
		return result;
	}
}

static Datum _longArray_coerceObject(Type self, jobject longArray)
{
	ArrayType* v;
	jsize nElems;

	if(longArray == 0)
		return 0;

	char* csig = PgObject_getClassName( JNI_getObjectClass(longArray) );

	nElems = JNI_getArrayLength((jarray)longArray);	

	if(csig[1] != '[') {
		
		v = createArrayType(nElems, sizeof(jlong), INT8OID, false);
		
		JNI_getFloatArrayRegion((jlongArray)longArray, 0,
						nElems, (jlong*)ARR_DATA_PTR(v));

		PG_RETURN_ARRAYTYPE_P(v);

	} else {

		if(csig[2] == '[')
			elog(ERROR,"Higher dimensional arrays not supported");
		
		jarray arr = (jarray) JNI_getObjectArrayElement(longArray,0); 
 
		jsize dim2;
		if(arr == 0) {
			dim2 = 0;
			nElems = 0;
		} else 
			dim2 = JNI_getArrayLength( arr );	

		v = create2dArrayType(nElems, dim2, sizeof(jlong), INT8OID, false);

		if(nElems > 0) {
			// Copy first dim
			JNI_getLongArrayRegion((jlongArray)arr, 0,
							dim2, (jlong*)ARR_DATA_PTR(v));
			
			// Copy remaining
			for(int i = 1; i < nElems; i++) {
				jlongArray els = JNI_getObjectArrayElement((jarray)longArray,i);
		
				JNI_getLongArrayRegion(els, 0,
							dim2, (jlong*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jlong)) );
			}
		}
		PG_RETURN_ARRAYTYPE_P(v);
	}
}

/*
 * java.lang.Long type.
 */
static bool _Long_canReplace(Type self, Type other)
{
	TypeClass cls = Type_getClass(other);
	return Type_getClass(self) == cls || cls == s_longClass;
}

static jvalue _Long_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = JNI_newObject(s_Long_class, s_Long_init, DatumGetInt64(arg));
	return result;
}

static Datum _Long_coerceObject(Type self, jobject longObj)
{
	return _asDatum(longObj == 0 ? 0 : JNI_callLongMethod(longObj, s_Long_longValue));
}

static Type _long_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid2(arrayTypeId, self, _longArray_coerceDatum, _longArray_coerceObject);
}

/* Make this datatype available to the postgres system.
 */
extern void Long_initialize(void);
void Long_initialize(void)
{
	Type t_long;
	Type t_Long;
	TypeClass cls;

	s_Long_class = JNI_newGlobalRef(PgObject_getJavaClass("java/lang/Long"));
	s_Long_init = PgObject_getJavaMethod(s_Long_class, "<init>", "(J)V");
	s_Long_longValue = PgObject_getJavaMethod(s_Long_class, "longValue", "()J");

	cls = TypeClass_alloc("type.Long");
	cls->canReplaceType = _Long_canReplace;
	cls->JNISignature = "Ljava/lang/Long;";
	cls->javaTypeName = "java.lang.Long";
	cls->coerceDatum  = _Long_coerceDatum;
	cls->coerceObject = _Long_coerceObject;
	t_Long = TypeClass_allocInstance(cls, INT8OID);

	cls = TypeClass_alloc("type.long");
	cls->JNISignature = "J";
	cls->javaTypeName = "long";
	cls->invoke       = _long_invoke;
	cls->coerceDatum  = _long_coerceDatum;
	cls->coerceObject = _Long_coerceObject;
	cls->createArrayType = _long_createArrayType;
	s_longClass = cls;

	t_long = TypeClass_allocInstance(cls, INT8OID);
	t_long->objectType = t_Long;
	Type_registerType("long", t_long);
	Type_registerType("java.lang.Long", t_Long);
}
