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

static TypeClass s_doubleClass;
static jclass    s_Double_class;
static jmethodID s_Double_init;
static jmethodID s_Double_doubleValue;

/*
 * double primitive type.
 */
static Datum _asDatum(jdouble v)
{
	MemoryContext currCtx = Invocation_switchToUpperContext();
	Datum ret = Float8GetDatum(v);
	MemoryContextSwitchTo(currCtx);
	return ret;
}

static Datum _double_invoke(Type self, Function fn, PG_FUNCTION_ARGS)
{
	return _asDatum(pljava_Function_doubleInvoke(fn));
}

static jvalue _double_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.d = DatumGetFloat8(arg);
	return result;
}

static jvalue _doubleArray_coerceDatum(Type self, Datum arg)
{
	jvalue     result;
	ArrayType* v      = DatumGetArrayTypeP(arg);
	
	if (ARR_NDIM(v) != 2 ) { 
		jsize      nElems = (jsize)ArrayGetNItems(ARR_NDIM(v), ARR_DIMS(v));
		jdoubleArray doubleArray = JNI_newDoubleArray(nElems);
		
		if(ARR_HASNULL(v))
		{
			jsize idx;
			jboolean isCopy = JNI_FALSE;
			bits8* nullBitMap = ARR_NULLBITMAP(v);
			jdouble* values = (jdouble*)ARR_DATA_PTR(v);
			jdouble* elems  = JNI_getDoubleArrayElements(doubleArray, &isCopy);
			for(idx = 0; idx < nElems; ++idx)
			{
				if(arrayIsNull(nullBitMap, idx))
					elems[idx] = 0;
				else
					elems[idx] = *values++;
			}
			JNI_releaseDoubleArrayElements(doubleArray, elems, JNI_COMMIT);
		}
		else {
			JNI_setDoubleArrayRegion(doubleArray, 0, nElems, (jdouble*)ARR_DATA_PTR(v));
			
			result.l = (jobject)doubleArray;
			return result;
		}
	} else {
		// Create outer array
		jobjectArray objArray = JNI_newObjectArray(ARR_DIMS(v)[0], JNI_newGlobalRef(PgObject_getJavaClass("[D")), 0);
		
		int nc = 0;
		int NaNc = 0;

		if(ARR_HASNULL(v)) {
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jdoubleArray innerArray = JNI_newDoubleArray(ARR_DIMS(v)[1]);
				
				
				jboolean isCopy = JNI_FALSE;
				bits8* nullBitMap = ARR_NULLBITMAP(v);

				jdouble* elems  = JNI_getDoubleArrayElements(innerArray, &isCopy);
			
				for(int jdx = 0; jdx < ARR_DIMS(v)[1]; ++jdx) {
					if(arrayIsNull(nullBitMap, nc)) {
						elems[jdx] = NAN;
						NaNc++;
					}
					else {
						elems[jdx] = *((jdouble*) (ARR_DATA_PTR(v)+(nc-NaNc)*sizeof(double)));
					}
					nc++;
				}
				JNI_releaseDoubleArrayElements(innerArray, elems, JNI_COMMIT);
	
				// Set
				JNI_setObjectArrayElement(objArray, idx, innerArray);
				JNI_deleteLocalRef(innerArray);
			}	
		} else {
				
			for (int idx = 0; idx < ARR_DIMS(v)[0]; ++idx)
			{
				// Create inner
				jdoubleArray innerArray = JNI_newDoubleArray(ARR_DIMS(v)[1]);
				
				JNI_setDoubleArrayRegion(innerArray, 0, ARR_DIMS(v)[1], (jdouble *) (ARR_DATA_PTR(v) + nc*sizeof(double) ));
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

static Datum _doubleArray_coerceObject(Type self, jobject doubleArray)
{	
	ArrayType* v;
	jsize nElems;

	if(doubleArray == 0)
		return 0;

	char* csig = PgObject_getClassName( JNI_getObjectClass(doubleArray) );

	nElems = JNI_getArrayLength((jarray)doubleArray);	

	if(csig[1] != '[') {
		
		v = createArrayType(nElems, sizeof(jdouble), FLOAT8OID, false);
		
		JNI_getDoubleArrayRegion((jdoubleArray)doubleArray, 0,
						nElems, (jdouble*)ARR_DATA_PTR(v));

		PG_RETURN_ARRAYTYPE_P(v);

	} else {
		
		if(csig[2] == '[')
			elog(ERROR,"Higher dimensional arrays not supported");

		jarray arr = (jarray) JNI_getObjectArrayElement(doubleArray,0); 
 		jsize dim2 = JNI_getArrayLength( arr );	

		v = create2dArrayType(nElems, dim2, sizeof(jdouble), FLOAT8OID, false);

		// Copy first dim
		JNI_getDoubleArrayRegion((jdoubleArray)arr, 0,
						dim2, (jdouble*)ARR_DATA_PTR(v));
		
		// Copy remaining
		for(int i = 1; i < nElems; i++) {
			jdoubleArray els = JNI_getObjectArrayElement((jarray)doubleArray,i);
	
			JNI_getDoubleArrayRegion(els, 0,
						dim2, (jdouble*) (ARR_DATA_PTR(v)+i*dim2*sizeof(jdouble)) );
		}

		PG_RETURN_ARRAYTYPE_P(v);
	}

}

/*
 * java.lang.Double type.
 */
static bool _Double_canReplace(Type self, Type other)
{
	TypeClass cls = Type_getClass(other);
	return Type_getClass(self) == cls || cls == s_doubleClass;
}

static jvalue _Double_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = JNI_newObject(s_Double_class, s_Double_init, DatumGetFloat8(arg));
	return result;
}

static Datum _Double_coerceObject(Type self, jobject doubleObj)
{
	return _asDatum(doubleObj == 0 ? 0 : JNI_callDoubleMethod(doubleObj, s_Double_doubleValue));
}

static Type _double_createArrayType(Type self, Oid arrayTypeId)
{
	return Array_fromOid2(arrayTypeId, self, _doubleArray_coerceDatum, _doubleArray_coerceObject);
}

/* Make this datatype available to the postgres system.
 */
extern void Double_initialize(void);
void Double_initialize(void)
{
	Type t_double;
	Type t_Double;
	TypeClass cls;

	s_Double_class = JNI_newGlobalRef(PgObject_getJavaClass("java/lang/Double"));
	s_Double_init = PgObject_getJavaMethod(s_Double_class, "<init>", "(D)V");
	s_Double_doubleValue = PgObject_getJavaMethod(s_Double_class, "doubleValue", "()D");

	cls = TypeClass_alloc("type.Double");
	cls->canReplaceType = _Double_canReplace;
	cls->JNISignature = "Ljava/lang/Double;";
	cls->javaTypeName = "java.lang.Double";
	cls->coerceDatum  = _Double_coerceDatum;
	cls->coerceObject = _Double_coerceObject;
	t_Double = TypeClass_allocInstance(cls, FLOAT8OID);

	cls = TypeClass_alloc("type.double");
	cls->JNISignature = "D";
	cls->javaTypeName = "double";
	cls->invoke       = _double_invoke;
	cls->coerceDatum  = _double_coerceDatum;
	cls->coerceObject = _Double_coerceObject;
	cls->createArrayType = _double_createArrayType;
	s_doubleClass = cls;

	t_double = TypeClass_allocInstance(cls, FLOAT8OID);
	t_double->objectType = t_Double;
	Type_registerType("double", t_double);
	Type_registerType("java.lang.Double", t_Double);
}
