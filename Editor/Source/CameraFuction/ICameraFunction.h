#pragma once

class FCamera;
class ICameraFunction
{
protected:
	FCamera* Camera = nullptr;
public:
	void SetCamera(FCamera* InCamera) { Camera = InCamera; }
	virtual void Tick(float DeltaTime) = 0;
};

