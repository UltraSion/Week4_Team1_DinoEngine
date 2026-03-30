#pragma once
#include "CoreMinimal.h"
#include "Object/Object.h"
#include "World/LevelTypes.h"

// Forward declarations ? include 최소화
class ULevel;
class AActor;
class UCameraComponent;
class FCamera;
class FFrustum;
struct FRenderCommandQueue;
struct ID3D11Device;

class ENGINE_API UWorld : public UObject
{
public:
	DECLARE_RTTI(UWorld, UObject)
	~UWorld();

	template <typename T>
	T* SpawnActor(const FString& InName);
	void DestroyActor(AActor* InActor);

	// ── Persistent Level ──
	ULevel* GetPersistentLevel() const { return PersistentLevel; }
	// ── Streaming Levels ──
	ULevel* LoadStreamingLevel(const FString& LevelName, ID3D11Device* Device = nullptr);
	void UnloadStreamingLevel(const FString& LevelName);
	ULevel* FindStreamingLevel(const FString& LevelName) const;
	const TArray<ULevel*>& GetStreamingLevels() const { return StreamingLevels; }

	// ── 전체 액터 조회 (Persistent + Streaming 합산) ──
	TArray<AActor*> GetAllActors() const;
	const TArray<AActor*>& GetActors() const;  // PersistentLevel만

	ULevel* GetLevel() const { return PersistentLevel; }
	// 카메라
	void SetActiveCameraComponent(UCameraComponent* InCamera);
	UCameraComponent* GetActiveCameraComponent() const;
	FCamera* GetCamera() const;

	// 라이프사이클
	void InitializeWorld(float AspectRatio, ID3D11Device* Device = nullptr);
	void BeginPlay();
	void Tick(float InDeltaTime);
	void CleanupWorld();
	
	ELevelType GetWorldType() const { return WorldType; }
	void SetWorldType(ELevelType InType) { WorldType = InType; }
	float GetWorldTime() const { return WorldTime; }
	float GetDeltaTime() const { return DeltaSeconds; }

private:
	ULevel* PersistentLevel = nullptr;      
	TArray<ULevel*> StreamingLevels;

	bool bBegunPlay = false;
	float WorldTime = 0.f;
	float DeltaSeconds = 0.f;
	ELevelType WorldType = ELevelType::Game;
	UCameraComponent* LevelCameraComponent = nullptr;    
	TObjectPtr<UCameraComponent> ActiveCameraComponent;
};
#include "World/Level.h"

template <typename T>
T* UWorld::SpawnActor(const FString& InName)
{
	static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");
	if (!PersistentLevel) return nullptr;
	return PersistentLevel->SpawnActor<T>(InName);
}
