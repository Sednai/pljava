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

static TypeClass s_intClass;
static jclass    s_Integer_class;
static jmethodID s_Integer_init;
static jmethodID s_Integer_intValue;

/*
 * int primitive type.
 */
static Datum _int_invoke(Type self, Function fn, PG_FUNCTION_ARGS)
{
	jint iv = pljava_Function_intInvoke(fn);
	return Int32GetDatum(iv);
}

static jvalue _int_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.i = DatumGetInt32(arg);
	return result;
}

static jvalue _intArray_coerceDatum(Type self, Datum arg)
{
	jvalue     result;
	ArrayType* v        = DatumGetArrayTypeP(arg);

	if (ARR_NDIM(v) != 2 ) {
		jsize      nElems   = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
		jintArray  intArray = JNI_newIntArray(nElems);

		if(ARR_HASNULL(v))
		{
			jsize idx;
			jboolean isCopy = JNI_FALSE;
			bits8* nullBitMap = ARR_NULLBITMAP(v);
			jint* values = (jint*)ARR_DATA_PTR(v);
			jint* elems  = JNI_getIntArrayElements(intArray, &isCopy);
			for(idx = 0; idx < nElems; ++idx)
			{
				if(arrayIsNull(nullBitMap, idx))
					elems[idx] = 0;
				else
					elems[idx] = *values++;
			}
			JNI_releaseIntArrayElements(intArray, elems, JNI_COMMIT);
		}
		else
			JNI_setIntArrayRegion(intArray, 0, nElems, (jint*)ARR_DATA_PTR(v));
	
		result.l = (jobject)intArray;
		return result;
	
	} else {

		// Create outer array
		jobjectArray objArray = JNI_newObjectArray(ARR_DIMS(v)[0], JNI_newGlobalRef(PgObject_getJavaClass("[I")), 0);
		
		int nc = 0;
		int NaNc = 0;

		if(ARR_HASNULL(v)) {
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jintArray innerArray = JNI_newIntArray(ARR_DIMS(v)[1]);
							
				jboolean isCopy = JNI_FALSE;
				bits8* nullBitMap = ARR_NULLBITMAP(v);

				jint* elems  = JNI_getIntArrayElements(innerArray, &isCopy);
			
				for(int jdx = 0; jdx < ARR_DIMS(v)[1]; ++jdx) {
					if(arrayIsNull(nullBitMap, nc)) {
						elems[jdx] = 0;
						NaNc++;
					}
					else {
						elems[jdx] = *((jfloat*) (ARR_DATA_PTR(v)+(nc-NaNc)*sizeof(int)));
					}
					nc++;
				}
				JNI_releaseIntArrayElements(innerArray, elems, JNI_COMMIT);
	
				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);
			}	

		} else {
				
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jintArray innerArray = JNI_newIntArray(ARR_DIMS(v)[1]);
				
				JNI_setIntArrayRegion(innerArray, 0, ARR_DIMS(v)[1], (jint *) (ARR_DATA_PTR(v) + nc*sizeof(int) ));
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

static Datum _intArray_coerceObject(Type self, jobject intArray)
{
	ArrayType* v;
	jsize nElems;

	if(intArray == 0)
		return 0;

	char* csig = PgObject_getClassName( JNI_getObjectClass(intArray) );

	nElems = JNI_getArrayLength((jarray)intArray);	

	if(csig[1] != '[') {
		
		v = createArrayType(nElems, sizeof(jfloat), INT4OID, false);
		
		JNI_getFloatArrayRegion((jintArray)intArray, 0,
						nElems, (jint*)ARR_DATA_PTR(v));

		PG_RETURN_ARRAYTYPE_P(v);

	} else {

		if(csig[2] == '[')
			elog(ERROR,"Higher dimensional arrays not supported");

		jarray arr = (jarray) JNI_getObjectArrayElement(intArray,0); 

		jsize dim2;
		if(arr == 0) {
			dim2 = 0;
			nElems = 1;
		} else 
			dim2 = JNI_getArrayLength( arr );	

		v = create2dArrayType(nElems, dim2, sizeof(jint), INT4OID, false);

		if(dim2 > 0) {
			// Copy first dim
			JNI_getIntArrayRegion((jintArray)arr, 0,
							dim2, (jint*)ARR_DATA_PTR(v));
			
			// Copy remaining
			for(int i = 1; i < nElems; i++) {
				jintArray els = JNI_getObjectArrayElement((jarray)intArray,i);
		
				JNI_getIntArrayRegion(els, 0,
							dim2, (jint*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jint)) );
			}
		}
		PG_RETURN_ARRAYTYPE_P(v);
	}

}

/*
 * java.lang.Integer type.
 */
static bool _Integer_canReplace(Type self, Type other)
{
	TypeClass cls = Type_getClass(other);
	return Type_getClass(self) == cls || cls == s_intClass;
}

static jvalue _Integer_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = JNI_newObject(s_Integer_class, s_Integer_init, DatumGetInt32(arg));
	return result;
}

static Datum _Integer_coerceObject(Type self, jobject intObj)
{
	return Int32GetDatum(intObj == 0 ? 0 : JNI_callIntMethod(intObj, s_Integer_intValue));
}

static Type _int_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid2(arrayTypeId, self, _intArray_coerceDatum, _intArray_coerceObject);
}

/* Make this datatype available to the postgres system.
 */
extern void Integer_initialize(void);
void Integer_initialize(void)
{
	Type t_int;
	Type t_Integer;
	TypeClass cls;

	s_Integer_class = JNI_newGlobalRef(PgObject_getJavaClass("java/lang/Integer"));
	s_Integer_init = PgObject_getJavaMethod(s_Integer_class, "<init>", "(I)V");
	s_Integer_intValue = PgObject_getJavaMethod(s_Integer_class, "intValue", "()I");

	cls = TypeClass_alloc("type.Integer");
	cls->canReplaceType = _Integer_canReplace;
	cls->JNISignature = "Ljava/lang/Integer;";
	cls->javaTypeName = "java.lang.Integer";
	cls->coerceDatum  = _Integer_coerceDatum;
	cls->coerceObject = _Integer_coerceObject;
	t_Integer = TypeClass_allocInstance(cls, INT4OID);

	cls = TypeClass_alloc("type.int");
	cls->JNISignature = "I";
	cls->javaTypeName = "int";
	cls->invoke       = _int_invoke;
	cls->coerceDatum  = _int_coerceDatum;
	cls->coerceObject = _Integer_coerceObject;
	cls->createArrayType = _int_createArrayType;
	s_intClass = cls;

	t_int = TypeClass_allocInstance(cls, INT4OID);
	t_int->objectType = t_Integer;
	Type_registerType("int", t_int);
	Type_registerType("java.lang.Integer", t_Integer);
}
