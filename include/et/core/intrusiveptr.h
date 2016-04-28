/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <atomic>
#include <type_traits>

namespace et
{
#	define ET_DECLARE_POINTER(T)					using Pointer = et::IntrusivePtr<T>;
	
	ObjectFactory& sharedObjectFactory();
    
    class Shared
    {
    public:
        size_t retain()
            { return _retainCount.operator ++(); }
        
        size_t release()
            { return _retainCount.operator --(); }
        
        size_t retainCount() const
            { return _retainCount.load(); }
        
    private:
        std::atomic<size_t> _retainCount;
    };

	template <typename T>
	class IntrusivePtr
	{
        static_assert(std::is_base_of<Shared, T>::value,
            "Intrusive pointer content must be derived from Shared class");
        
	public:
		IntrusivePtr() = default;

		IntrusivePtr(const IntrusivePtr& r)
			{ reset(r._data);  }

		IntrusivePtr(IntrusivePtr&& r)
			{ std::swap(_data, r._data); }
		
		template <typename R>
		IntrusivePtr(IntrusivePtr<R> r)
			{ reset(static_cast<T*>(r.ptr()));  }

		explicit IntrusivePtr(T* data)
			{ reset(data); }
			
		virtual ~IntrusivePtr() 
			{ reset(nullptr); }
		
#if (ET_SUPPORT_VARIADIC_TEMPLATES)
		
		template <typename ...args>
		static IntrusivePtr create(args&&...a)
			{ return IntrusivePtr<T>(sharedObjectFactory().createObject<T>(a...)); }
		
#else
#
#	error Please compile with variadic templates enabled
#
#endif
		
		T* operator *()
			{ ET_ASSERT(valid()); return _data; }

		const T* operator *() const 
			{ ET_ASSERT(valid()); return _data; }

		T* operator -> ()
			{ ET_ASSERT(valid()); return _data; }

		const T* operator -> () const 
			{ ET_ASSERT(valid()); return _data; }

		T* ptr()
			{ return _data; }

		const T* ptr() const 
			{ return _data; }

		T& reference()
			{ ET_ASSERT(valid()); return *_data; }

		const T& reference() const
			{ ET_ASSERT(valid()); return *_data; }

		bool invalid() const
			{ return _data == nullptr; }

		bool valid() const 
			{ return _data != nullptr; }

		bool operator == (const IntrusivePtr& r) const
			{ return _data == r._data; }

		bool operator != (const IntrusivePtr& tr) const
			{ return _data != tr._data; }

		bool operator < (const IntrusivePtr& tr) const
			{ return _data < tr._data; }
		
		size_t retainCount() const
			{ return _data ? _data->retainCount() : 0; }

		IntrusivePtr<T>& operator = (const IntrusivePtr<T>& r)
		{ 
			reset(r._data); 
			return *this; 
		}

		template <typename R>
		IntrusivePtr<T>& operator = (IntrusivePtr<R> r)
		{
			reset(static_cast<T*>(r.ptr()));
			return *this;
		}

		void reset(T* data) 
		{
			if (data == _data) return;
            
			if ((_data != nullptr) && (_data->release() == 0))
				sharedObjectFactory().deleteObject<T>(_data);
            
            std::swap(_data, data);

			if (_data != nullptr)
				_data->retain();
		}
		
		void swap(IntrusivePtr<T>& t)
			{ std::swap(_data, t._data); }

	private:
		T* _data = nullptr;
	};

}
