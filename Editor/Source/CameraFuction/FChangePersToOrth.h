#pragma once
#include "ICameraFunction.h"
#include "CoreMinimal.h"

class FChangePersToOrth : public ICameraFunction
{
private:
	// ===== Projection 상태 =====

	// 시작 시 Perspective FOV 값
	float StartFOV = 0.0f;

	// 목표 Orthographic Width
	float TargetOrthoWidth = 0.0f;

	// 시작 위치 (카메라 위치 보간용)
	FVector StartPosition;

	// 목표 위치 (Ortho 기준 프레이밍 유지용)
	FVector TargetPosition;


	// ===== 시간 제어 =====

	// 전체 전환 시간
	float TransitionTime = 1.0f;

	// 현재 경과 시간
	float ElapsedTime = 0.0f;

	// 현재 전환 진행 여부
	bool bIsTransitioning = false;


	// ===== 내부 상태 =====

	// Projection이 Ortho로 전환되었는지 여부 (중간 스위칭 제어)
	bool bSwitchedToOrtho = false;


public:

	/**
	 * @brief Perspective → Orthographic 전환 시작
	 *
	 * @param InTargetPosition  목표 카메라 위치
	 * @param InOrthoWidth     목표 Orthographic Width
	 * @param InTransitionTime 전환 시간
	 */
	void StartTransition(const FVector& InTargetPosition, float InOrthoWidth, float InTransitionTime);


	/**
	 * @brief 매 프레임 업데이트 (Tick)
	 */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsFinished() const override { return !bIsTransitioning; }


private:

	/**
	 * @brief EaseInOut 보간 함수
	 */
	float EvaluateEaseInOut(float T) const;
};
