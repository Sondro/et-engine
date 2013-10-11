/*
 * This file is part of `et engine`
 * Copyright 2009-2013 by Sergey Reznik
 * Please, do not modify content without approval.
 *
 */

#pragma once

#include <et/core/et.h>

#define ET_SINGLETON_CONSTRUCTORS(t)	ET_DENY_COPY(t) friend class et::Singleton<t>; t() { }
#define ET_SINGLETON_COPY_DENY(t)		ET_DENY_COPY(t) friend class et::Singleton<t>;

namespace et
{ 
	template <class t_class>
	class Singleton
	{
	public:
		static t_class& instance()
		{
			static t_class _instance;
			return _instance;
		}

	protected:
		Singleton() { };
		ET_DENY_COPY(Singleton)
	};
}
