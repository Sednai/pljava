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

static TypeClass s_floatClass;
static jclass    s_Float_class;
static jmethodID s_Float_init;
static jmethodID s_Float_floatValue;

/*
 * float primitive type.
 */
static Datum _asDatum(jfloat v)
{
	MemoryContext currCtx = Invocation_switchToUpperContext();
	Datum ret = Float4GetDatum(v);
	MemoryContextSwitchTo(currCtx);
	return ret;
}

static Datum _float_invoke(Type self, Function fn, PG_FUNCTION_ARGS)
{
	return _asDatum(pljava_Function_floatInvoke(fn));
}

static jvalue _float_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.f = DatumGetFloat4(arg);
	return result;
}

static jvalue _floatArray_coerceDatum(Type self, Datum arg)
{	
	jvalue     result;
	ArrayType* v      = DatumGetArrayTypeP(arg);

	if (ARR_NDIM(v) != 2 ) { 
		jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
		jfloatArray floatArray = JNI_newFloatArray(nElems);

		if(ARR_HASNULL(v))
		{
			jsize idx;
			jboolean isCopy = JNI_FALSE;
			bits8* nullBitMap = ARR_NULLBITMAP(v);
			jfloat* values = (jfloat*)ARR_DATA_PTR(v);
			jfloat* elems  = JNI_getFloatArrayElements(floatArray, &isCopy);
			for(idx = 0; idx < nElems; ++idx)
			{
				if(arrayIsNull(nullBitMap, idx))
					elems[idx] = 0;
				else
					elems[idx] = *values++;
			}
			JNI_releaseFloatArrayElements(floatArray, elems, JNI_COMMIT);
		} else {
			JNI_setFloatArrayRegion(floatArray, 0, nElems, (jfloat *)ARR_DATA_PTR(v));
		}

		result.l = (jobject)floatArray;
		return result;
	}
	else {
		
		// Create outer array
		jobjectArray objArray = JNI_newObjectArray(ARR_DIMS(v)[0], JNI_newGlobalRef(PgObject_getJavaClass("[F")), 0);
		
		int nc = 0;
		int NaNc = 0;

		if(ARR_HASNULL(v)) {
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jfloatArray innerArray = JNI_newFloatArray(ARR_DIMS(v)[1]);
							
				jboolean isCopy = JNI_FALSE;
				bits8* nullBitMap = ARR_NULLBITMAP(v);

				jfloat* elems  = JNI_getFloatArrayElements(innerArray, &isCopy);
			
				for(int jdx = 0; jdx < ARR_DIMS(v)[1]; ++jdx) {
					if(arrayIsNull(nullBitMap, nc)) {
						elems[jdx] = NAN;
						NaNc++;
					}
					else {
						elems[jdx] = *((jfloat*) (ARR_DATA_PTR(v)+(nc-NaNc)*sizeof(float)));
					}
					nc++;
				}
				JNI_releaseFloatArrayElements(innerArray, elems, JNI_COMMIT);
	
				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);
			}	
		} else {
				
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jfloatArray innerArray = JNI_newFloatArray(ARR_DIMS(v)[1]);
				
				JNI_setFloatArrayRegion(innerArray, 0, ARR_DIMS(v)[1], (jfloat *) (ARR_DATA_PTR(v) + nc*sizeof(float) ));
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

static Datum _floatArray_coerceObject(Type self, jobject floatArray)
{
	ArrayType* v;
	jsize nElems;

	if(floatArray == 0)
		return 0;

	char* csig = PgObject_getClassName( JNI_getObjectClass(floatArray) );

	nElems = JNI_getArrayLength((jarray)floatArray);	

	if(csig[1] != '[') {
		
		v = createArrayType(nElems, sizeof(jfloat), FLOAT4OID, false);
		
		JNI_getFloatArrayRegion((jfloatArray)floatArray, 0,
						nElems, (jfloat*)ARR_DATA_PTR(v));

		PG_RETURN_ARRAYTYPE_P(v);

	} else {
		
		if(csig[2] == '[')
			elog(ERROR,"Higher dimensional arrays not supported");

		jarray arr = (jarray) JNI_getObjectArrayElement(floatArray,0); 

		jsize dim2;
		if(arr == 0) {
			dim2 = 0;
			nElems = 1;
		} else 
			dim2 = JNI_getArrayLength( arr );	

		v = create2dArrayType(nElems, dim2, sizeof(jfloat), FLOAT4OID, false);

		if(dim2 > 0) {
			// Copy first dim
			JNI_getFloatArrayRegion((jfloatArray)arr, 0,
							dim2, (jfloat*)ARR_DATA_PTR(v));
			
			// Copy remaining
			for(int i = 1; i < nElems; i++) {
				jfloatArray els = JNI_getObjectArrayElement((jarray)floatArray,i);
		
				JNI_getFloatArrayRegion(els, 0,
							dim2, (jfloat*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jfloat)) );
			}
		}

		PG_RETURN_ARRAYTYPE_P(v);
	}
}

/*
 * java.lang.Float type.
 */
static bool _Float_canReplace(Type self, Type other)
{
	TypeClass cls = Type_getClass(other);
	return Type_getClass(self) == cls || cls == s_floatClass;
}

static jvalue _Float_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = JNI_newObject(s_Float_class, s_Float_init, DatumGetFloat4(arg));
	return result;
}

static Datum _Float_coerceObject(Type self, jobject floatObj)
{
	return _asDatum(floatObj == 0 ? 0.0f : JNI_callFloatMethod(floatObj, s_Float_floatValue));
}

static Type _float_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid2(arrayTypeId, self, _floatArray_coerceDatum, _floatArray_coerceObject);
}

/* Make this datatype available to the postgres system.
 */
extern void Float_initialize(void);
void Float_initialize(void)
{
	Type t_float;
	Type t_Float;
	TypeClass cls;

	s_Float_class = JNI_newGlobalRef(PgObject_getJavaClass("java/lang/Float"));
	s_Float_init = PgObject_getJavaMethod(s_Float_class, "<init>", "(F)V");
	s_Float_floatValue = PgObject_getJavaMethod(s_Float_class, "floatValue", "()F");

	cls = TypeClass_alloc("type.Float");
	cls->canReplaceType = _Float_canReplace;
	cls->JNISignature = "Ljava/lang/Float;";
	cls->javaTypeName = "java.lang.Float";
	cls->coerceDatum  = _Float_coerceDatum;
	cls->coerceObject = _Float_coerceObject;
	t_Float = TypeClass_allocInstance(cls, FLOAT4OID);

	cls = TypeClass_alloc("type.float");
	cls->JNISignature = "F";
	cls->javaTypeName = "float";
	cls->invoke       = _float_invoke;
	cls->coerceDatum  = _float_coerceDatum;
	cls->coerceObject = _Float_coerceObject;
	cls->createArrayType = _float_createArrayType;
	s_floatClass = cls;

	t_float = TypeClass_allocInstance(cls, FLOAT4OID);
	t_float->objectType = t_Float;
	Type_registerType("float", t_float);
	Type_registerType("java.lang.Float", t_Float);
}
