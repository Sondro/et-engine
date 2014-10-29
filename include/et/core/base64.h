/*
 * This file is part of `et engine`
 * Copyright 2009-2014 by Sergey Reznik
 * Please, do not modify content without approval.
 *
 */

#pragma once

#include <et/core/datastorage.h>

namespace et
{
	namespace base64
	{
		size_t decodedDataSize(const std::string&);
		BinaryDataStorage decode(const std::string&);
		std::string encode(const BinaryDataStorage&);
	}
}
