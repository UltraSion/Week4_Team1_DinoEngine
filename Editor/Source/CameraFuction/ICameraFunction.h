#pragma once

class FCamera;
class ICameraFunction
{
protected:
	FCamera* Camera = nullptr;
public:
	virtual bool IsFinished() const = 0;
	void SetCamera(FCamera* InCamera) { Camera = InCamera; }
	virtual void Tick(float DeltaTime) = 0;
};

