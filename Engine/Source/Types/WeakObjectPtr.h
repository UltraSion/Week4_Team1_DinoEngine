#pragma once
#include "Object/Object.h"
#include <type_traits>

template<typename T = UObject>
struct TWeakObjectPtr
{
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");

	int32 ObjectIndex = -1;
	uint32 ObjectSerialNumber = 0;

	TWeakObjectPtr() = default;

	TWeakObjectPtr(T* Object)
	{
		if (Object && !Object->IsPendingKill())
		{
			ObjectIndex = static_cast<int32>(Object->InternalIndex);
			ObjectSerialNumber = Object->SerialNumber;
		}
	}

	// 포인터가 아직 유효한지 검사 (핵심 로직)
	bool IsValid() const
	{
		if (ObjectIndex < 0 || ObjectIndex >= static_cast<int32>(GUObjectArray.size()))
			return false;

		UObject* Obj = GUObjectArray[ObjectIndex];
		// 슬롯에 객체가 있고, 일련번호가 내가 알던 그 번호가 맞으며, 삭제 대기 상태가 아닐 때만 true
		if (Obj && Obj->SerialNumber == ObjectSerialNumber && !Obj->IsPendingKill())
		{
			return true;
		}
		return false;
	}

	// 유효하면 포인터 반환, 아니면 nullptr 반환
	T* Get() const
	{
		return IsValid() ? static_cast<T*>(GUObjectArray[ObjectIndex]) : nullptr;
	}

	T* operator->() const { return Get(); }
	explicit operator bool() const { return IsValid(); }

	TWeakObjectPtr& operator=(T* Object)
	{
		*this = TWeakObjectPtr(Object);
		return *this;
	}
};