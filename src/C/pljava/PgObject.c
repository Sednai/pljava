/*
 * This file contains software that has been made available under The BSD
 * license. Use and distribution hereof are subject to the restrictions set
 * forth therein.
 * 
 * Copyright (c) 2003 TADA AB - Taby Sweden
 * All Rights Reserved
 */
#include <postgres.h>
#include <executor/spi.h>

#include "pljava/PgObject_priv.h"

void PgObject_free(PgObject object)
{
	Finalizer finalizer = object->m_class->finalize;
	if(finalizer != 0)
		finalizer(object);
	pfree(object);
}

PgObject PgObjectClass_allocInstance(PgObjectClass clazz, MemoryContext ctx)
{
	Size sz = clazz->instanceSize;
	PgObject infant = (PgObject)MemoryContextAlloc(ctx, sz);
	memset(infant, 0, sz);
	infant->m_class = clazz;
	return infant;
}

void PgObjectClass_init(PgObjectClass clazz, const char* name, Size instanceSize, Finalizer finalizer)
{
	clazz->name = name;
	clazz->instanceSize = instanceSize;
	clazz->finalize = finalizer;
}

PgObjectClass PgObjectClass_create(const char* name, Size instanceSize, Finalizer finalizer)
{
	PgObjectClass self = (PgObjectClass)MemoryContextAlloc(TopMemoryContext, sizeof(struct PgObjectClass_));
	memset(self, 0, sizeof(struct PgObjectClass_));
	PgObjectClass_init(self, name, instanceSize, finalizer);
	return self;
}

void _PgObject_pureVirtualCalled(PgObject object)
{
	ereport(ERROR, (errmsg("Pure virtual method called")));
}

void PgObject_throwMemberError(const char* memberName, const char* signature, bool isMethod, bool isStatic)
{
	ereport(ERROR, (
		errmsg("Unable to find%s %s %s with signature %s",
			(isStatic ? " static" : ""),
			(isMethod ? "method" : "field"),
			memberName,
			signature)));
}	

jclass PgObject_getJavaClass(JNIEnv* env, const char* className)
{
	jclass cls;
	bool saveIcj = isCallingJava;
	isCallingJava = true;
	cls = (*env)->FindClass(env, className);
	isCallingJava = saveIcj;

	if(cls == 0)
	{
		(*env)->ExceptionDescribe(env);
		ereport(ERROR, (
			errmsg("Unable to load class %s using CLASSPATH '%s'",
				className,
				getenv("CLASSPATH"))));
	}
	return cls;
}

jmethodID PgObject_getJavaMethod(JNIEnv* env, jclass cls, const char* methodName, const char* signature)
{
	jmethodID m;
	bool saveIcj = isCallingJava;
	isCallingJava = true;
	m = (*env)->GetMethodID(env, cls, methodName, signature);
	isCallingJava = saveIcj;

	if(m == 0)
		PgObject_throwMemberError(methodName, signature, true, false);
	return m;
}
	
jmethodID PgObject_getStaticJavaMethod(JNIEnv* env, jclass cls, const char* methodName, const char* signature)
{
	jmethodID m;
	bool saveIcj = isCallingJava;
	
	isCallingJava = true;
	m = (*env)->GetStaticMethodID(env, cls, methodName, signature);
	isCallingJava = saveIcj;

	if(m == 0)
		PgObject_throwMemberError(methodName, signature, true, true);
	return m;
}
	
jfieldID PgObject_getJavaField(JNIEnv* env, jclass cls, const char* fieldName, const char* signature)
{
	jfieldID m;
	bool saveIcj = isCallingJava;
	isCallingJava = true;
	m = (*env)->GetFieldID(env, cls, fieldName, signature);
	isCallingJava = saveIcj;

	if(m == 0)
		PgObject_throwMemberError(fieldName, signature, false, false);
	return m;
}

jfieldID PgObject_getStaticJavaField(JNIEnv* env, jclass cls, const char* fieldName, const char* signature)
{
	jfieldID m;
	bool saveIcj = isCallingJava;
	isCallingJava = true;
	m = (*env)->GetStaticFieldID(env, cls, fieldName, signature);
	isCallingJava = saveIcj;

	if(m == 0)
		PgObject_throwMemberError(fieldName, signature, false, true);
	return m;
}

jobject PgObject_newJavaObject(JNIEnv* env, jclass cls, jmethodID ctor, ...)
{
	jobject obj;
	va_list args;
	bool saveIcj = isCallingJava;

	va_start(args, ctor);
	isCallingJava = true;
	obj = (*env)->NewObjectV(env, cls, ctor, args);
	isCallingJava = saveIcj;
	va_end(args);

	return obj;
}

HeapTuple PgObject_getValidTuple(int cacheId, Oid tupleId, const char* tupleType)
{
	HeapTuple tuple = SearchSysCache(cacheId, ObjectIdGetDatum(tupleId), 0, 0, 0);
	if(!HeapTupleIsValid(tuple))
		ereport(ERROR, (errmsg("cache lookup failed for %s %u", tupleType, tupleId)));
	return tuple;
}
