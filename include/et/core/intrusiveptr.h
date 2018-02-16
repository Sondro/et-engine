/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <atomic>
#include <type_traits>

namespace et {
#	define ET_DECLARE_POINTER(T)					using Pointer = et::IntrusivePtr<T>
#	define ET_FORWARD_DECLARE_POINTER(T)			class T; using T##Pointer = et::IntrusivePtr<T>

ObjectFactory& sharedObjectFactory();

class Shared
{
public:
	Shared() = default;
	Shared(Shared&&) = delete;
	Shared(const Shared&) = delete;
	Shared& operator = (const Shared&) = delete;

	uint_fast32_t retain() {
	#if (ET_DEBUG)
		if (_trackRetains) debug::debugBreak();
	#endif
		return ++_retainCount;
	}

	uint_fast32_t release() {
	#if (ET_DEBUG)
		if (_trackRetains) debug::debugBreak();
	#endif
		return --_retainCount;
	}

	uint_fast32_t retainCount() const {
		return _retainCount.load();
	}

#if (ET_DEBUG)
	void enableRetainCycleTracking(bool enabled) { _trackRetains = enabled ? 1 : 0; }
#else
	void enableRetainCycleTracking(bool) {}
#endif

private:
	std::atomic_uint_fast32_t _retainCount{ 0 };
#if (ET_DEBUG)
	uint32_t _trackRetains = 0;
#endif
};

enum class PointerInit : uint32_t
{
	WithNullptr,
	CreateInplace
};

template <class T>
class InstusivePointerScope
{
public:
	InstusivePointerScope(T* object) : _object(object) {
		_object->retain();
	}

	~InstusivePointerScope() {
		_object->release();
	}

	InstusivePointerScope(const InstusivePointerScope&) = delete;
	InstusivePointerScope(InstusivePointerScope&&) = delete;
	InstusivePointerScope& operator = (const InstusivePointerScope&) = delete;

private:
	T * _object = nullptr;
};

template <typename T>
class IntrusivePtr
{
	static_assert(std::is_base_of<Shared, T>::value,
		"Intrusive pointer content must be derived from Shared class");

public:
	IntrusivePtr() = default;

	IntrusivePtr(PointerInit initType) {
		if (initType == PointerInit::CreateInplace)
			reset(sharedObjectFactory().createObject<T>());
	}

	IntrusivePtr(const IntrusivePtr& r) :
		_data(r._data) {
		if (_data != nullptr)
			_data->retain();
	}

	IntrusivePtr(IntrusivePtr&& r) :
		_data(r._data) {
		r._data = nullptr;
	}

	template <typename R>
	IntrusivePtr(IntrusivePtr<R> r) :
		_data(static_cast<T*>(r.pointer())) {
		r.removeObject();
	}

	explicit IntrusivePtr(T* data) :
		_data(data) {
		if (_data != nullptr)
			_data->retain();
	}

	~IntrusivePtr() {
		if (_data != nullptr)
			release(_data);
	}

#if (ET_SUPPORT_VARIADIC_TEMPLATES)

	template <typename ...args>
	static IntrusivePtr create(args&&...a) {
		return IntrusivePtr<T>(sharedObjectFactory().createObject<T>(std::forward<args>(a)...));
	}

#else
#
#	error Please compile with variadic templates enabled
#
#endif

	T* operator *() {
		ET_ASSERT(valid()); return _data;
	}

	const T* operator *() const {
		ET_ASSERT(valid()); return _data;
	}

	T* operator -> () {
		ET_ASSERT(valid()); return _data;
	}

	const T* operator -> () const {
		ET_ASSERT(valid()); return _data;
	}

	T* pointer() {
		return _data;
	}

	const T* pointer() const {
		return _data;
	}

	T& reference() {
		ET_ASSERT(valid()); return *_data;
	}

	const T& reference() const {
		ET_ASSERT(valid()); return *_data;
	}

	bool invalid() const {
		return _data == nullptr;
	}

	bool valid() const {
		return _data != nullptr;
	}

	bool operator == (const IntrusivePtr& r) const {
		return _data == r._data;
	}

	bool operator != (const IntrusivePtr& tr) const {
		return _data != tr._data;
	}

	bool operator < (const IntrusivePtr& tr) const {
		return _data < tr._data;
	}

	uint_fast32_t retainCount() const {
		return _data ? _data->retainCount() : 0;
	}

	IntrusivePtr<T>& operator = (const IntrusivePtr<T>& r) {
		reset(r._data);
		return *this;
	}

	template <typename R>
	IntrusivePtr<T>& operator = (IntrusivePtr<R> r) {
		reset(static_cast<T*>(r.pointer()));
		return *this;
	}

	void reset(T* newData) {
		if (newData == _data)
			return;

		if (newData != nullptr)
			newData->retain();

		T* oldData = _data;
		std::swap(_data, newData);

		if (oldData != nullptr)
			release(oldData);
	}

	void release() {
		release(_data);
		_data = nullptr;
	}

	void removeObject() {
		_data = nullptr;
	}

	void swap(IntrusivePtr<T>& t) {
		std::swap(_data, t._data);
	}

private:
	void release(T* object) {
		ET_ASSERT(object != nullptr);
		ET_ASSERT(object->retainCount() > 0);

		if (object->release() == 0)
			sharedObjectFactory().deleteObject<T>(object);
	}

private:
	T * _data = nullptr;
};

}
