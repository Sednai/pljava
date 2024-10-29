/*
 * Copyright (c) 2004-2022 Tada AB and other contributors, as listed below.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the The BSD 3-Clause License
 * which accompanies this distribution, and is available at
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Contributors:
 *   Tada AB
 *   Chapman Flack
 *
 * @author Thomas Hallgren
 */
#include <postgres.h>
#include <executor/spi.h>
#include <executor/tuptable.h>

#include "org_postgresql_pljava_internal_Tuple.h"
#include "pljava/Backend.h"
#include "pljava/DualState.h"
#include "pljava/Exception.h"
#include "pljava/type/Type_priv.h"
#include "pljava/type/Tuple.h"
#include "pljava/type/TupleDesc.h"

#include <time.h>

static jclass    s_Tuple_class;
static jmethodID s_Tuple_init;

enum { NS_PER_SECOND = 1000000000 };

void sub_timespec(struct timespec t1, struct timespec t2, struct timespec *td)
{
    td->tv_nsec = t2.tv_nsec - t1.tv_nsec;
    td->tv_sec  = t2.tv_sec - t1.tv_sec;
    if (td->tv_sec > 0 && td->tv_nsec < 0)
    {
        td->tv_nsec += NS_PER_SECOND;
        td->tv_sec--;
    }
    else if (td->tv_sec < 0 && td->tv_nsec > 0)
    {
        td->tv_nsec -= NS_PER_SECOND;
        td->tv_sec++;
    }
}
/*
 * org.postgresql.pljava.type.Tuple type.
 */
jobject pljava_Tuple_create(HeapTuple ht)
{
	jobject jht = 0;
	if(ht != 0)
	{
		MemoryContext curr = MemoryContextSwitchTo(JavaMemoryContext);
		jht = pljava_Tuple_internalCreate(ht, true);
		MemoryContextSwitchTo(curr);
	}
	return jht;
}

jobjectArray pljava_Tuple_createArray(HeapTuple* vals, jint size, bool mustCopy)
{
	jobjectArray tuples = JNI_newObjectArray(size, s_Tuple_class, 0);

	/*
	struct timespec start, finish, delta;
    clock_gettime(CLOCK_REALTIME, &start);
	int tmp = size;
	*/
	while(--size >= 0)
	{
		jobject heapTuple = pljava_Tuple_internalCreate(vals[size], mustCopy);
		JNI_setObjectArrayElement(tuples, size, heapTuple);
		JNI_deleteLocalRef(heapTuple);
	}
		
/*
	clock_gettime(CLOCK_REALTIME, &finish);
    sub_timespec(start, finish, &delta);

	elog(WARNING,"[DEBUG](heapCopy): %d.%.9ld (%d)",(int)delta.tv_sec, delta.tv_nsec,tmp);
*/
	return tuples;
}

jobject pljava_Tuple_internalCreate(HeapTuple ht, bool mustCopy)
{
	jobject jht;
	Ptr2Long htH;
	
	if(mustCopy)
		ht = heap_copytuple(ht);

	htH.longVal = 0L; /* ensure that the rest is zeroed out */
	htH.ptrVal = ht;
	/*
	 * XXX? this seems like a lot of tuple copying.
	 */
	jht = JNI_newObjectLocked(s_Tuple_class, s_Tuple_init,
		htH.longVal);
	return jht;
}

static jvalue _Tuple_coerceDatum(Type self, Datum arg)
{
	jvalue result;
	result.l = pljava_Tuple_create((HeapTuple)DatumGetPointer(arg));
	return result;
}

/* Make this datatype available to the postgres system.
 */
extern void pljava_Tuple_initialize(void);
void pljava_Tuple_initialize(void)
{
	TypeClass cls;
	JNINativeMethod methods[] = {
		{
		"_getObject",
		"(JJILjava/lang/Class;)Ljava/lang/Object;",
	  	Java_org_postgresql_pljava_internal_Tuple__1getObject
		},
		{ 0, 0, 0 }};

	s_Tuple_class = JNI_newGlobalRef(PgObject_getJavaClass("org/postgresql/pljava/internal/Tuple"));
	PgObject_registerNatives2(s_Tuple_class, methods);
	s_Tuple_init = PgObject_getJavaMethod(s_Tuple_class, "<init>",
		"(J)V");

	cls = TypeClass_alloc("type.Tuple");
	cls->JNISignature = "Lorg/postgresql/pljava/internal/Tuple;";
	cls->javaTypeName = "org.postgresql.pljava.internal.Tuple";
	cls->coerceDatum  = _Tuple_coerceDatum;
	Type_registerType("org.postgresql.pljava.internal.Tuple", TypeClass_allocInstance(cls, InvalidOid));
}

jobject
pljava_Tuple_getObject(
	TupleDesc tupleDesc, HeapTuple tuple, int index, jclass rqcls)
{
	jobject result = 0;
	PG_TRY();
	{
		Type type = pljava_TupleDesc_getColumnType(tupleDesc, index);
		if(type != 0)
		{
			bool wasNull = false;
			Datum binVal = SPI_getbinval(tuple, tupleDesc, (int)index, &wasNull);
			if(!wasNull)
				result = Type_coerceDatumAs(type, binVal, rqcls).l;
		}
	}
	PG_CATCH();
	{
		Exception_throw_ERROR("SPI_getbinval");
	}
	PG_END_TRY();
	return result;
}

/****************************************
 * JNI methods
 ****************************************/
 
/*
 * Class:     org_postgresql_pljava_internal_Tuple
 * Method:    _getObject
 * Signature: (JJILjava/lang/Class;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL
Java_org_postgresql_pljava_internal_Tuple__1getObject(JNIEnv* env, jclass cls, jlong _this, jlong _tupleDesc, jint index, jclass rqcls)
{
	jobject result = 0;
	Ptr2Long p2l;
	p2l.longVal = _this;

	BEGIN_NATIVE
	HeapTuple self = (HeapTuple)p2l.ptrVal;
	p2l.longVal = _tupleDesc;
	result = pljava_Tuple_getObject((TupleDesc)p2l.ptrVal, self, (int)index, rqcls);
	END_NATIVE
	return result;
}
