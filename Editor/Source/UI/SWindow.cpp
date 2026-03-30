#include "SWindow.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <stdexcept>
#include <windowsx.h>
#include "CoreMinimal.h"
#include "Windows.h"
#include "imgui.h"

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

	ImGuiWindowFlags GetSplitterOverlayWindowFlags()
	{
		return
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoBackground;
	}

	ImVec2 ToScreenPosition(const FPoint& Position)
	{
		const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
		if (MainViewport == nullptr)
		{
			return ImVec2(Position.X, Position.Y);
		}

		return ImVec2(MainViewport->Pos.x + Position.X, MainViewport->Pos.y + Position.Y);
	}

	ImU32 GetSplitterColor(bool bHovered, bool bActive)
	{
		if (bActive)
		{
			return IM_COL32(110, 170, 255, 220);
		}

		if (bHovered)
		{
			return IM_COL32(150, 150, 150, 200);
		}

		return IM_COL32(90, 90, 90, 160);
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
	if (InNewWindow == nullptr)
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
	if (Parent)
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
	{
		InSideLT->SetParent(this);
		InSideLT->SetRect(GetSideLTRect());
	}
}

void SSplitter::SetSideRB(SWindow* InSideRB)
{
	SideRB = InSideRB;
	if (InSideRB)
	{
		InSideRB->SetParent(this);
		InSideRB->SetRect(GetSideRBRect());
	}
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

float SSplitter::ClampSplitRatio(float InSplitRatio) const
{
	const float AvailableAxis = GetAvailablePrimaryAxisSize();
	if (AvailableAxis <= 0.0f)
	{
		return 0.5f;
	}

	const float ClampedMinPaneSize = (std::max)(0.0f, MinPaneSize);
	float MinRatio = ClampedMinPaneSize / AvailableAxis;
	float MaxRatio = 1.0f - MinRatio;

	if (MinRatio >= MaxRatio)
	{
		return 0.5f;
	}

	return (std::clamp)(InSplitRatio, MinRatio, MaxRatio);
}

float SSplitter::GetAvailablePrimaryAxisSize() const
{
	return (std::max)(0.0f, GetPrimaryAxisSize() - SplitterThickness);
}

void SSplitter::InitializeSplitterLayout()
{
	SplitRatio = ClampSplitRatio(SplitRatio);

	if (SideLT)
	{
		SideLT->SetParent(this);
		SideLT->SetRect(GetSideLTRect());
	}

	if (SideRB)
	{
		SideRB->SetParent(this);
		SideRB->SetRect(GetSideRBRect());
	}
}

void SSplitter::SetSplitRatio(float InSplitRatio)
{
	const float ClampedSplitRatio = ClampSplitRatio(InSplitRatio);
	if (SplitRatio == ClampedSplitRatio)
	{
		return;
	}

	SplitRatio = ClampedSplitRatio;
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

void SSplitter::Render()
{
	if (SideLT)
	{
		SideLT->Render();
	}
	if (SideRB)
	{
		SideRB->Render();
	}
}

void SSplitter::Draw()
{
	const float ClampedSplitRatio = ClampSplitRatio(SplitRatio);
	if (SplitRatio != ClampedSplitRatio)
	{
		SplitRatio = ClampedSplitRatio;
		OnResize();
	}

	if (SideLT)
	{
		SideLT->Draw();
	}
	if (SideRB)
	{
		SideRB->Draw();
	}

	DrawSplitterHandle();
}

bool SSplitter::HandleMessage(FCore* Core, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	if (bDragging && IsMouseMessage(Msg))
	{
		return true;
	}

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

void SSplitter::DrawSplitterHandle()
{
	const FRect SplitterRect = GetSplitterRect();
	if (SplitterRect.Size.X <= 0.0f || SplitterRect.Size.Y <= 0.0f)
	{
		bDragging = false;
		return;
	}

	char OverlayName[64];
	std::snprintf(OverlayName, sizeof(OverlayName), "##SplitterOverlay_%p", static_cast<void*>(this));

	ImGui::SetNextWindowPos(ToScreenPosition(SplitterRect.Position), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(SplitterRect.Size.X, SplitterRect.Size.Y), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::Begin(OverlayName, nullptr, GetSplitterOverlayWindowFlags()))
	{
		ImGui::PushID(this);
		ImGui::InvisibleButton("##Handle", ImVec2(SplitterRect.Size.X, SplitterRect.Size.Y));

		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		bDragging = bActive;

		if (bHovered || bActive)
		{
			ImGui::SetMouseCursor(GetSplitterMouseCursor());
		}

		if (bActive)
		{
			const float AvailableAxis = GetAvailablePrimaryAxisSize();
			if (AvailableAxis > 0.0f)
			{
				// SplitRatio = FirstPaneSize / AvailableAxis.
				// Converting drag delta from pixels to ratio is just delta / AvailableAxis.
				const float DeltaRatio = GetMouseDeltaForSplit() / AvailableAxis;
				SetSplitRatio(SplitRatio + DeltaRatio);
			}
			else
			{
				SetSplitRatio(0.5f);
			}
		}

		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		DrawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), GetSplitterColor(bHovered, bActive));

		ImGui::PopID();
	}

	ImGui::End();
	ImGui::PopStyleVar(2);
}

FRect SSplitterV::GetSideLTRect()
{
	const float AvailableHeight = (std::max)(0.0f, Rect.Size.Y - SplitterThickness);
	const float Height = AvailableHeight * SplitRatio;
	return FRect({ Rect.Position.X, Rect.Position.Y }, { Rect.Size.X, Height });
}

FRect SSplitterV::GetSideRBRect()
{
	const float AvailableHeight = (std::max)(0.0f, Rect.Size.Y - SplitterThickness);
	const float TopHeight = AvailableHeight * SplitRatio;
	const float BottomHeight = AvailableHeight - TopHeight;
	return FRect(
		{ Rect.Position.X, Rect.Position.Y + TopHeight + SplitterThickness },
		{ Rect.Size.X, BottomHeight });
}

FRect SSplitterH::GetSideLTRect()
{
	const float AvailableWidth = (std::max)(0.0f, Rect.Size.X - SplitterThickness);
	const float Width = AvailableWidth * SplitRatio;
	return FRect({ Rect.Position.X, Rect.Position.Y }, { Width, Rect.Size.Y });
}

FRect SSplitterH::GetSideRBRect()
{
	const float AvailableWidth = (std::max)(0.0f, Rect.Size.X - SplitterThickness);
	const float LeftWidth = AvailableWidth * SplitRatio;
	const float RightWidth = AvailableWidth - LeftWidth;
	return FRect(
		{ Rect.Position.X + LeftWidth + SplitterThickness, Rect.Position.Y },
		{ RightWidth, Rect.Size.Y });
}

float SSplitterV::GetPrimaryAxisSize() const
{
	return Rect.Size.Y;
}

float SSplitterV::GetMouseDeltaForSplit() const
{
	return ImGui::GetIO().MouseDelta.y;
}

ImGuiMouseCursor SSplitterV::GetSplitterMouseCursor() const
{
	return ImGuiMouseCursor_ResizeNS;
}

FRect SSplitterV::GetSplitterRect() const
{
	const float AvailableHeight = (std::max)(0.0f, Rect.Size.Y - SplitterThickness);
	const float TopHeight = AvailableHeight * SplitRatio;
	return FRect(
		{ Rect.Position.X, Rect.Position.Y + TopHeight },
		{ Rect.Size.X, SplitterThickness });
}

void SSplitterV::Draw()
{
	SSplitter::Draw();
}

float SSplitterH::GetPrimaryAxisSize() const
{
	return Rect.Size.X;
}

float SSplitterH::GetMouseDeltaForSplit() const
{
	return ImGui::GetIO().MouseDelta.x;
}

ImGuiMouseCursor SSplitterH::GetSplitterMouseCursor() const
{
	return ImGuiMouseCursor_ResizeEW;
}

FRect SSplitterH::GetSplitterRect() const
{
	const float AvailableWidth = (std::max)(0.0f, Rect.Size.X - SplitterThickness);
	const float LeftWidth = AvailableWidth * SplitRatio;
	return FRect(
		{ Rect.Position.X + LeftWidth, Rect.Position.Y },
		{ SplitterThickness, Rect.Size.Y });
}

void SSplitterH::Draw()
{
	SSplitter::Draw();
}
