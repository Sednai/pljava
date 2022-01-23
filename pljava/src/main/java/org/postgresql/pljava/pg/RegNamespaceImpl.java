/*
 * Copyright (c) 2022 Tada AB and other contributors, as listed below.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the The BSD 3-Clause License
 * which accompanies this distribution, and is available at
 * http://opensource.org/licenses/BSD-3-Clause
 *
 * Contributors:
 *   Chapman Flack
 */
package org.postgresql.pljava.pg;

import java.lang.invoke.MethodHandle;
import static java.lang.invoke.MethodHandles.lookup;
import java.lang.invoke.SwitchPoint;

import java.sql.SQLException;

import java.util.List;

import java.util.function.UnaryOperator;

import org.postgresql.pljava.internal.SwitchPointCache.Builder;

import org.postgresql.pljava.model.*;

import org.postgresql.pljava.pg.CatalogObjectImpl.*;
import static org.postgresql.pljava.pg.ModelConstants.NAMESPACEOID; // syscache

import org.postgresql.pljava.pg.adt.GrantAdapter;
import org.postgresql.pljava.pg.adt.NameAdapter;
import static org.postgresql.pljava.pg.adt.OidAdapter.REGROLE_INSTANCE;

import org.postgresql.pljava.sqlgen.Lexicals.Identifier.Simple;
import org.postgresql.pljava.sqlgen.Lexicals.Identifier.Unqualified;

class RegNamespaceImpl extends Addressed<RegNamespace>
implements
	Nonshared<RegNamespace>, Named<Simple>, Owned,
	AccessControlled<CatalogObject.Grant.OnNamespace>, RegNamespace
{
	private static UnaryOperator<MethodHandle[]> s_initializer;

	/* Implementation of Addressed */

	@Override
	public RegClass.Known<RegNamespace> classId()
	{
		return CLASSID;
	}

	@Override
	int cacheId()
	{
		return NAMESPACEOID;
	}

	/* Implementation of Named, Owned, AccessControlled */

	private static Simple name(RegNamespaceImpl o) throws SQLException
	{
		TupleTableSlot t = o.cacheTuple();
		return
			t.get(t.descriptor().get("nspname"), NameAdapter.SIMPLE_INSTANCE);
	}

	private static RegRole owner(RegNamespaceImpl o) throws SQLException
	{
		TupleTableSlot t = o.cacheTuple();
		return t.get(t.descriptor().get("nspowner"), REGROLE_INSTANCE);
	}

	private static List<CatalogObject.Grant> grants(RegNamespaceImpl o)
	throws SQLException
	{
		TupleTableSlot t = o.cacheTuple();
		return t.get(t.descriptor().get("nspacl"), GrantAdapter.LIST_INSTANCE);
	}

	/* Implementation of RegNamespace */

	/**
	 * Merely passes the supplied slots array to the superclass constructor; all
	 * initialization of the slots will be the responsibility of the subclass.
	 */
	RegNamespaceImpl()
	{
		super(s_initializer.apply(new MethodHandle[NSLOTS]));
	}

	static
	{
		s_initializer =
			new Builder<>(RegNamespaceImpl.class)
			.withLookup(lookup())
			.withSwitchPoint(o -> s_globalPoint[0])
			.withSlots(o -> o.m_slots)
			.withCandidates(RegNamespaceImpl.class.getDeclaredMethods())

			.withReceiverType(CatalogObjectImpl.Named.class)
			.withReturnType(Unqualified.class)
			.withDependent(      "name", SLOT_NAME)
			.withReturnType(null)
			.withReceiverType(CatalogObjectImpl.Owned.class)
			.withDependent(     "owner", SLOT_OWNER)
			.withReceiverType(CatalogObjectImpl.AccessControlled.class)
			.withDependent(    "grants", SLOT_ACL)

			.build()
			/*
			 * Add these slot initializers after what Addressed does.
			 */
			.compose(CatalogObjectImpl.Addressed.s_initializer)::apply;
	}
}
