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

static TypeClass s_booleanClass;
static jclass    s_Boolean_class;
static jmethodID s_Boolean_init;
static jmethodID s_Boolean_booleanValue;

/*
 * boolean primitive type.
 */
static Datum _boolean_invoke(Type self, Function fn, PG_FUNCTION_ARGS)
{
	jboolean v = pljava_Function_booleanInvoke(fn);
	return BoolGetDatum(v);
}

static jvalue _boolean_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.z = DatumGetBool(arg);
	return result;
}

static jvalue _booleanArray_coerceDatum(Type self, Datum arg)
{
	jvalue     result;
	ArrayType* v      = DatumGetArrayTypeP(arg);
	
	if (ARR_NDIM(v) != 2 ) { 
		jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
		jbooleanArray booleanArray = JNI_newBooleanArray(nElems);

		if(ARR_HASNULL(v))
		{
			jsize idx;
			jboolean isCopy = JNI_FALSE;
			bits8* nullBitMap = ARR_NULLBITMAP(v);
			jboolean* values = (jboolean*)ARR_DATA_PTR(v);
			jboolean* elems  = JNI_getBooleanArrayElements(booleanArray, &isCopy);
			for(idx = 0; idx < nElems; ++idx)
			{
				if(arrayIsNull(nullBitMap, idx))
					elems[idx] = 0;
				else
					elems[idx] = *values++;
			}
			JNI_releaseBooleanArrayElements(booleanArray, elems, JNI_COMMIT);
		}
		else
			JNI_setBooleanArrayRegion(booleanArray, 0, nElems, (jboolean*)ARR_DATA_PTR(v));
		result.l = (jobject)booleanArray;
		return result;
	
	} else {
		
		// Create outer array
		jobjectArray objArray = JNI_newObjectArray(ARR_DIMS(v)[0], JNI_newGlobalRef(PgObject_getJavaClass("[Z")), 0);
		
		int nc = 0;
		int NaNc = 0;

		if(ARR_HASNULL(v)) {
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jbooleanArray innerArray = JNI_newBooleanArray(ARR_DIMS(v)[1]);
							
				jboolean isCopy = JNI_FALSE;
				bits8* nullBitMap = ARR_NULLBITMAP(v);

				jboolean* elems  = JNI_getBooleanArrayElements(innerArray, &isCopy);
			
				for(int jdx = 0; jdx < ARR_DIMS(v)[1]; ++jdx) {
					if(arrayIsNull(nullBitMap, nc)) {
						elems[jdx] = 0;
						NaNc++;
					}
					else {
						elems[jdx] = *((jboolean*) (ARR_DATA_PTR(v)+(nc-NaNc)*sizeof(bool)));
					}
					nc++;
				}
				JNI_releaseBooleanArrayElements(innerArray, elems, JNI_COMMIT);
	
				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);
			}	
		} else {
				
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jbooleanArray innerArray = JNI_newBooleanArray(ARR_DIMS(v)[1]);
				
				JNI_setBooleanArrayRegion(innerArray, 0, ARR_DIMS(v)[1], (jboolean *) (ARR_DATA_PTR(v) + nc*sizeof(bool) ));
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

static Datum _booleanArray_coerceObject(Type self, jobject booleanArray)
{
	ArrayType* v;
	jsize nElems;

	if(booleanArray == 0)
		return 0;

	char* csig = PgObject_getClassName( JNI_getObjectClass(booleanArray) );

	nElems = JNI_getArrayLength((jarray)booleanArray);

	if(csig[1] != '[') {
	
		v = createArrayType(nElems, sizeof(jboolean), BOOLOID, false);

		JNI_getBooleanArrayRegion((jbooleanArray)booleanArray, 0,
						nElems, (jboolean*)ARR_DATA_PTR(v));
	
	} else {
		
		if(csig[2] == '[')
			elog(ERROR,"Higher dimensional arrays not supported");

		jarray arr = (jarray) JNI_getObjectArrayElement(booleanArray,0); 
 		jsize dim2 = JNI_getArrayLength( arr );	

		v = create2dArrayType(nElems, dim2, sizeof(jboolean), BOOLOID, false);

		// Copy first dim
		JNI_getBooleanArrayRegion((jbooleanArray)arr, 0,
						dim2, (jboolean*)ARR_DATA_PTR(v));
		
		// Copy remaining
		for(int i = 1; i < nElems; i++) {
			jbooleanArray els = JNI_getObjectArrayElement((jarray)booleanArray,i);
	
			JNI_getBooleanArrayRegion(els, 0,
						dim2, (jboolean*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jboolean)) );
		}
	}

	PG_RETURN_ARRAYTYPE_P(v);
}

/*
 * java.lang.Boolean type.
 */
static bool _Boolean_canReplace(Type self, Type other)
{
	TypeClass cls = Type_getClass(other);
	return Type_getClass(self) == cls || cls == s_booleanClass;
}

static jvalue _Boolean_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = JNI_newObject(s_Boolean_class, s_Boolean_init, DatumGetBool(arg));
	return result;
}

static Datum _Boolean_coerceObject(Type self, jobject booleanObj)
{
	return BoolGetDatum(booleanObj == 0 ? false : JNI_callBooleanMethod(booleanObj, s_Boolean_booleanValue) == JNI_TRUE);
}

static Type _boolean_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid2(arrayTypeId, self, _booleanArray_coerceDatum, _booleanArray_coerceObject);
}

/* Make this datatype available to the postgres system.
 */
extern void Boolean_initialize(void);
void Boolean_initialize(void)
{
	Type t_boolean;
	Type t_Boolean;
	TypeClass cls;

	s_Boolean_class = JNI_newGlobalRef(PgObject_getJavaClass("java/lang/Boolean"));
	s_Boolean_init = PgObject_getJavaMethod(s_Boolean_class, "<init>", "(Z)V");
	s_Boolean_booleanValue = PgObject_getJavaMethod(s_Boolean_class, "booleanValue", "()Z");

	cls = TypeClass_alloc("type.Boolean");
	cls->canReplaceType = _Boolean_canReplace;
	cls->JNISignature = "Ljava/lang/Boolean;";
	cls->javaTypeName = "java.lang.Boolean";
	cls->coerceDatum  = _Boolean_coerceDatum;
	cls->coerceObject = _Boolean_coerceObject;
	t_Boolean = TypeClass_allocInstance(cls, BOOLOID);

	cls = TypeClass_alloc("type.boolean");
	cls->JNISignature = "Z";
	cls->javaTypeName = "boolean";
	cls->invoke       = _boolean_invoke;
	cls->coerceDatum  = _boolean_coerceDatum;
	cls->coerceObject = _Boolean_coerceObject;
	cls->createArrayType = _boolean_createArrayType;
	s_booleanClass = cls;

	t_boolean = TypeClass_allocInstance(cls, BOOLOID);
	t_boolean->objectType = t_Boolean;

	Type_registerType("boolean", t_boolean);
	Type_registerType("java.lang.Boolean", t_Boolean);
}
