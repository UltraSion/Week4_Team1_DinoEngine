#include "SWindow.h"
#include <stdexcept>
#include <windowsx.h>
#include "CoreMinimal.h"
#include "Windows.h"

namespace
{
	bool IsMouseMessage(UINT Msg)
	{
		switch (Msg)
		{
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			return true;
		default:
			return false;
		}
	}
}

void SWindow::SetRect(const FRect& InRect)
{
	Rect = InRect;
	OnResize();
}

bool SWindow::ISHover(FPoint coord) const
{
	return coord.X >= Rect.Position.X && coord.X <= Rect.Position.X + Rect.Size.X &&
		coord.Y >= Rect.Position.Y && coord.Y <= Rect.Position.Y + Rect.Size.Y;
}

SWindow* SWindow::GetWindow(FPoint coord)
{
	if (ISHover(coord))
	{
		return this;
	}

	return nullptr;
}

SSplitter* SWindow::Split(SWindow* InNewWindow, SplitDirection InDirection, SplitOption InSplitOption)
{
	if(InNewWindow == nullptr)
		return nullptr;

	SSplitter* NewSplitter = nullptr;

	if (InDirection == SplitDirection::Horizontal)
	{
		NewSplitter = new SSplitterH(GetRect());
	}
	else if (InDirection == SplitDirection::Vertical)
	{
		NewSplitter = new SSplitterV(GetRect());
	}

	if (!NewSplitter)
		return nullptr;

	NewSplitter->SetParent(Parent);
	if(Parent)
	{
		Parent->ReplaceSide(this, NewSplitter);
	}

	if (InSplitOption == SplitOption::LT)
	{
		NewSplitter->SetSideLT(this);
		NewSplitter->SetSideRB(InNewWindow);
	}
	else if (InSplitOption == SplitOption::RB)
	{
		NewSplitter->SetSideLT(InNewWindow);
		NewSplitter->SetSideRB(this);
	}



	return NewSplitter;
}

void SSplitter::SetSideLT(SWindow* InSideLT)
{
	SideLT = InSideLT;
	if (InSideLT)
		InSideLT->SetParent(this);

	InSideLT->SetRect(GetSideLTRect());
}

void SSplitter::SetSideRB(SWindow* InSideRB)
{
	SideRB = InSideRB;
	if (InSideRB)
		InSideRB->SetParent(this);

	InSideRB->SetRect(GetSideRBRect());
}

void SSplitter::OnResize()
{
	if (SideLT)
	{
		SideLT->SetRect(GetSideLTRect());
	}
	if (SideRB)
	{
		SideRB->SetRect(GetSideRBRect());
	}
}

void SSplitter::ReplaceSide(SWindow* OldSide, SWindow* NewSide)
{
	if (OldSide == SideLT)
	{
		SetSideLT(NewSide);
		return;
	}
	
	if (OldSide == SideRB)
	{
		SetSideRB(NewSide);
		return;
	}
	
	throw std::runtime_error("OldSide is not part of this splitter.");
}

SSplitter::SSplitter(FRect InRect, SWindow* InSideLT, SWindow* InSideRB, float InSplitRatio)
	: SWindow(InRect), SideLT(InSideLT), SideRB(InSideRB), SplitRatio(InSplitRatio)
{
	SetSplitRatio(InSplitRatio);

	if(InSideLT)
		SetSideLT(InSideLT);

	if(InSideRB)
		SetSideRB(InSideRB);
}

SSplitter::~SSplitter()
{
	if (SideLT)
	{
		delete SideLT;
		SideLT = nullptr;
	}
	if (SideRB)
	{
		delete SideRB;
		SideRB = nullptr;
	}
}

SWindow* SSplitter::GetWindow(FPoint coord)
{
	if (SideLT && SideLT->ISHover(coord))
	{
		return SideLT->GetWindow(coord);
	}
	else if (SideRB && SideRB->ISHover(coord))
	{
		return SideRB->GetWindow(coord);
	}
	return nullptr;
}

void SSplitter::SetSplitRatio(float InSplitRatio)
{
	if (InSplitRatio < 0.0f)
	{
		SplitRatio = 0.0f;
	}
	else if (InSplitRatio > 1.0f)
	{
		SplitRatio = 1.0f;
	}
	else
	{
		SplitRatio = InSplitRatio;
	}
	OnResize();
}

void SSplitter::Tick(float DeltaTime)
{
	if (SideLT)
	{
		SideLT->Tick(DeltaTime);
	}
	if (SideRB)
	{
		SideRB->Tick(DeltaTime);
	}
}

void SSplitter::Draw()
{
	if (SideLT)
	{
		SideLT->Draw();
	}
	if (SideRB)
	{
		SideRB->Draw();
	}
}

bool SSplitter::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (IsMouseMessage(Msg))
	{
		const FPoint MousePoint(static_cast<float>(GET_X_LPARAM(LParam)), static_cast<float>(GET_Y_LPARAM(LParam)));
		if (SideLT && SideLT->ISHover(MousePoint))
		{
			return SideLT->HandleMessage(Core, Hwnd, Msg, WParam, LParam);
		}

		if (SideRB && SideRB->ISHover(MousePoint))
		{
			return SideRB->HandleMessage(Core, Hwnd, Msg, WParam, LParam);
		}

		return false;
	}

	if (SideLT && SideLT->HandleMessage(Core, Hwnd, Msg, WParam, LParam))
	{
		return true;
	}

	if (SideRB && SideRB->HandleMessage(Core, Hwnd, Msg, WParam, LParam))
	{
		return true;
	}

	return false;
}

FRect SSplitterH::GetSideLTRect()
{
	float Height = Rect.Size.Y * SplitRatio;
	return FRect({ Rect.Position.X, Rect.Position.Y }, { Rect.Size.X , Height });
}

FRect SSplitterH::GetSideRBRect()
{
	float Height = Rect.Size.Y * (1 - SplitRatio);
	return FRect({ Rect.Position.X, Rect.Position.Y + Height }, { Rect.Size.X , Height });
}

FRect SSplitterV::GetSideLTRect()
{
	float Width = Rect.Size.X * SplitRatio;
	return FRect({ Rect.Position.X, Rect.Position.Y }, { Width, Rect.Size.Y });
}

FRect SSplitterV::GetSideRBRect()
{
	float Width = Rect.Size.X * (1 - SplitRatio);
	return FRect({ Rect.Position.X + Width, Rect.Position.Y }, { Width, Rect.Size.Y });
}
