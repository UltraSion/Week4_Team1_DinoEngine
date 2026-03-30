#pragma once
template <typename T>
class TObjectIterator
{
	static_assert(std::is_base_of_v<UObject, T>, "T must derive from UObject");

	int32 Index;

	// T 타입인 유효 오브젝트 찾을 때까지 Index 전진
	void Advance()
	{
		while (Index < (int32)GUObjectArray.size())
		{
			UObject* Obj = GUObjectArray[Index];
			if (Obj && !Obj->IsPendingKill() && Obj->IsA<T>())
				return;
			++Index;
		}
	}

public:
	TObjectIterator() : Index(0) { Advance(); }

	explicit operator bool() const { return Index < (int32)GUObjectArray.size(); }
	T* operator*() const { return static_cast<T*>(GUObjectArray[Index]); }
	T* operator->() const { return static_cast<T*>(GUObjectArray[Index]); }
	TObjectIterator& operator++() { ++Index; Advance(); return *this; }
};


//GUObjectArray(Object.h에 extern 선언)를 순회
//Advance()에서 nullptr, PendingKill, 타입 불일치를 모두 스킵
//IsA<T>()는 이미 UObject에 구현되어 있음
//생성자에서 Advance() 호출 -> 첫 유효 오브젝트로 자동 이동
