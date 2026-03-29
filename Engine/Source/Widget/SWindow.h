#pragma once
#include "Math/Rect.h"

class FCore;
struct FRenderCommandQueue;

enum SplitDirection
{
	Horizontal,
	Vertical,
};

enum SplitOption
{
	LT,
	RB,
};

class SSplitter;

class ENGINE_API SWindow
{
	SSplitter* Parent = nullptr;

protected:
	FRect Rect;

public:
	virtual ~SWindow() = default;

	void SetParent(SSplitter* InParent) { Parent = InParent; }
	SSplitter* GetParent() const { return Parent; }
	void SetRect(const FRect& InRect);
	virtual bool ISHover(FPoint coord) const;
	virtual SWindow* GetWindow(FPoint coord);
	FRect GetRect() const { return Rect; }
	virtual void OnResize() {}
	virtual void Tick(FCore* Core, float DeltaTime) {}
	virtual void Draw(FCore* Core, FRenderCommandQueue& CommandQueue) {}
	SSplitter* Split(SWindow* InNewWindow, SplitDirection InDirection, SplitOption InSplitOption);
};

class ENGINE_API SSplitter : public SWindow
{
	virtual SWindow* GetWindow(FPoint coord) override;

protected:
	float SplitRatio = 0.5f;
	SWindow* SideLT = nullptr;
	SWindow* SideRB = nullptr;

public:
	SWindow* GetSideLT() { return SideLT; }
	SWindow* GetSideRB() { return SideRB; }
	virtual void SetSideLT(SWindow* InSideLT);
	virtual void SetSideRB(SWindow* InSideRB);
	virtual FRect GetSideLTRect() = 0;
	virtual FRect GetSideRBRect() = 0;
	virtual void OnResize() override;
	void ReplaceSide(SWindow* OldSide, SWindow* NewSide);
	SSplitter(SWindow* InSideLT = nullptr, SWindow* InSideRB = nullptr, float InSplitRatio = 0.5f);
	~SSplitter() override;
	float GetSplitRatio() { return SplitRatio; }
	virtual void SetSplitRatio(float InSplitRatio);
	virtual void Tick(FCore* Core, float DeltaTime) override;
	virtual void Draw(FCore* Core, FRenderCommandQueue& CommandQueue) override;
};

class SSplitterH : public SSplitter
{
public:
	SSplitterH(SWindow* InSideLT = nullptr, SWindow* InSideRB = nullptr, float InSplitRatio = 0.5f)
		: SSplitter(InSideLT, InSideRB, InSplitRatio) {}

	virtual FRect GetSideLTRect() override;
	virtual FRect GetSideRBRect() override;
};

class SSplitterV : public SSplitter
{
public:
	SSplitterV(SWindow* InSideLT = nullptr, SWindow* InSideRB = nullptr, float InSplitRatio = 0.5f)
		: SSplitter(InSideLT, InSideRB, InSplitRatio) {}

	virtual FRect GetSideLTRect() override;
	virtual FRect GetSideRBRect() override;
};

