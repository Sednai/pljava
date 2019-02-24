/*
 * Copyright (c) 2018-2019 Tada AB and other contributors, as listed below.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the The BSD 3-Clause License
 * which accompanies this distribution, and is available at
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Contributors:
 *   Thomas Hallgren
 *   Chapman Flack
 */

#include "org_postgresql_pljava_internal_DualState_SinglePfree.h"
#include "org_postgresql_pljava_internal_DualState_SingleMemContextDelete.h"
#include "pljava/DualState.h"

#include "pljava/PgObject.h"
#include "pljava/JNICalls.h"

/*
 * Includes for objects dependent on DualState, so they can be initialized here
 */
#include "pljava/VarlenaWrapper.h"

static jclass s_DualState_class;

static jmethodID s_DualState_resourceOwnerRelease;
static jmethodID s_DualState_cleanEnqueuedInstances;

static jobject s_DualState_key;

static void resourceReleaseCB(ResourceReleasePhase phase,
							  bool isCommit, bool isTopLevel, void *arg);

/*
 * Return a capability that is only expected to be accessible to native code.
 */
jobject pljava_DualState_key(void)
{
	return s_DualState_key;
}

/*
 * Rather than using finalizers (deprecated in recent Java anyway), which can
 * increase the number of threads needing to interact with PG, DualState objects
 * will be enqueued on a ReferenceQueue when their referents become unreachable,
 * and this function should be called from strategically-chosen points in native
 * code so the thread already interacting with PG will clean the enqueued items.
 */
void pljava_DualState_cleanEnqueuedInstances(void)
{
	JNI_callStaticVoidMethodLocked(s_DualState_class,
								   s_DualState_cleanEnqueuedInstances);
}

/*
 * Called when the lifespan/scope of a particular PG resource owner is about to
 * expire, to make the associated DualState objects inaccessible from Java. As
 * described in DualState.java, the argument will often be a PG ResourceOwner
 * (when this function is called by resourceReleaseCB), but pointers to other
 * structures can also be used (such a pointer clearly can't be confused with a
 * ResourceOwner existing at the same time). In PG 9.5+, it could be a
 * MemoryContext, with a MemoryContextCallback established to call this
 * function. For items whose scope is limited to a single PL/Java function
 * invocation, this can be a pointer to the Invocation.
 */
void pljava_DualState_nativeRelease(void *ro)
{
	Ptr2Long p2l;

	/*
	 * This static assertion does not need to be in every file
	 * that uses Ptr2Long, but it should be somewhere once, so here it is.
	 */
	StaticAssertStmt(sizeof p2l.ptrVal <= sizeof p2l.longVal,
					 "Pointer will not fit in long on this platform");

	p2l.longVal = 0L;
	p2l.ptrVal = ro;
	JNI_callStaticVoidMethodLocked(s_DualState_class,
								   s_DualState_resourceOwnerRelease,
								   p2l.longVal);
}

void pljava_DualState_initialize(void)
{
	jclass clazz;
	jmethodID ctor;

	JNINativeMethod singlePfreeMethods[] =
	{
		{
		"_pfree",
		"(J)V",
		Java_org_postgresql_pljava_internal_DualState_00024SinglePfree__1pfree
		},
		{ 0, 0, 0 }
	};

	JNINativeMethod singleMemContextDeleteMethods[] =
	{
		{
		"_memContextDelete",
		"(J)V",
		Java_org_postgresql_pljava_internal_DualState_00024SingleMemContextDelete__1memContextDelete
		},
		{ 0, 0, 0 }
	};

	s_DualState_class = (jclass)JNI_newGlobalRef(PgObject_getJavaClass(
		"org/postgresql/pljava/internal/DualState"));
	s_DualState_resourceOwnerRelease = PgObject_getStaticJavaMethod(
		s_DualState_class, "resourceOwnerRelease", "(J)V");
	s_DualState_cleanEnqueuedInstances = PgObject_getStaticJavaMethod(
		s_DualState_class, "cleanEnqueuedInstances", "()V");

	clazz = (jclass)PgObject_getJavaClass(
		"org/postgresql/pljava/internal/DualState$Key");
	ctor = PgObject_getJavaMethod(clazz, "<init>", "()V");
	s_DualState_key = JNI_newGlobalRef(JNI_newObject(clazz, ctor));
	JNI_deleteLocalRef(clazz);

	clazz = (jclass)PgObject_getJavaClass(
		"org/postgresql/pljava/internal/DualState$SinglePfree");
	PgObject_registerNatives2(clazz, singlePfreeMethods);
	JNI_deleteLocalRef(clazz);

	clazz = (jclass)PgObject_getJavaClass(
		"org/postgresql/pljava/internal/DualState$SingleMemContextDelete");
	PgObject_registerNatives2(clazz, singleMemContextDeleteMethods);
	JNI_deleteLocalRef(clazz);

	RegisterResourceReleaseCallback(resourceReleaseCB, NULL);

	/*
	 * Call initialize() methods of known classes built upon DualState.
	 */
	pljava_VarlenaWrapper_initialize();
}

static void resourceReleaseCB(ResourceReleasePhase phase,
							  bool isCommit, bool isTopLevel, void *arg)
{
	/*
	 * The way ResourceOwnerRelease is implemented, callbacks to loadable
	 * modules (like us!) happen /after/ all of the built-in releasey actions
	 * for a particular phase. So, by looking for RESOURCE_RELEASE_LOCKS here,
	 * we actually end up executing after all the built-in lock-related stuff
	 * has been released, but before any of the built-in stuff released in the
	 * RESOURCE_RELEASE_AFTER_LOCKS phase. Which, at least for the currently
	 * implemented DualState subclasses, is about the right time.
	 */
	if ( RESOURCE_RELEASE_LOCKS != phase )
		return;

	pljava_DualState_nativeRelease(CurrentResourceOwner);
}



/*
 * Class:     org_postgresql_pljava_internal_DualState_SinglePfree
 * Method:    _pfree
 * Signature: (J)V
 *
 * Cadged from JavaWrapper.c
 */
JNIEXPORT void JNICALL
Java_org_postgresql_pljava_internal_DualState_00024SinglePfree__1pfree(
	JNIEnv* env, jobject _this, jlong pointer)
{
	BEGIN_NATIVE_NO_ERRCHECK
	Ptr2Long p2l;
	p2l.longVal = pointer;
	pfree(p2l.ptrVal);
	END_NATIVE
}



/*
 * Class:     org_postgresql_pljava_internal_DualState_SingleMemContextDelete
 * Method:    _memContextDelete
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_org_postgresql_pljava_internal_DualState_00024SingleMemContextDelete__1memContextDelete(
	JNIEnv* env, jobject _this, jlong pointer)
{
	BEGIN_NATIVE_NO_ERRCHECK
	Ptr2Long p2l;
	p2l.longVal = pointer;
	MemoryContextDelete(p2l.ptrVal);
	END_NATIVE
}
