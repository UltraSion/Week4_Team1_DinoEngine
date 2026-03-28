#include "Core.h"

#include "Core/Paths.h"
#include "Core/ConsoleVariableManager.h"
#include "World/Level.h"
#include "Actor/Actor.h"
#include "Input/EnhancedInputManager.h"
#include "Object/ObjectFactory.h"
#include "Object/ObjectManager.h"
#include "Component/PrimitiveComponent.h"
#include "Primitive/PrimitiveBase.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/MaterialManager.h"
#include "Math/Frustum.h"
#include "Input/InputManager.h"
#include "ViewportClient.h"
#include "Object/ObjectGlobals.h"
#include "Component/UUIDBillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Actor/SkySphereActor.h"
#include "ViewportContext.h"

FCore::~FCore()
{
	Release();
}

bool FCore::Initialize(HWND Hwnd, int32 Width, int32 Height, ELevelType StartupLevelType)
{
	FPaths::Initialize();
	WindowWidth = Width;
	WindowHeight = Height;

	GRenderer = new FRenderer(Hwnd, Width, Height);


	if (!GRenderer)
	{
		return false;
	}

	ObjManager = new ObjectManager();

	FMaterialManager::Get().LoadAllMaterials(GRenderer->GetDevice(), GRenderer->GetRenderStateManager().get());

	//InputManager = new FInputManager();
	//EnhancedInput = new FEnhancedInputManager();

	PhysicsManager = std::make_unique<FPhysicsManager>();

	Timer.Initialize();
	RegisterConsoleVariables();
	LevelManager = std::make_unique<FLevelManager>();
	const float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	if (!LevelManager->Initialize(AspectRatio, StartupLevelType, GRenderer))
	{
		return false;
	}

	return true;
}

//void FCore::SetViewportClient(FViewportClient* InViewportClient)
//{
//	if (InViewportClient)
//	{
//		InViewportClient->Attach(this);
//		return;
//	}
//
//	if (GRenderer)
//	{
//		GRenderer->ClearViewportCallbacks();
//	}
//}
//
//void FCore::AddViewportClient(FViewportClient* InViewportClient)
//{
//	if (InViewportClient)
//	{
//		InViewportClient->Attach(this);
//	}
//}

//void FCore::ProcessInput(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
//{
//	if (InputManager)
//	{
//		InputManager->ProcessMessage(Hwnd, Msg, WParam, LParam);
//	}
//
//	if (MainViewportClient)
//	{
//		MainViewportClient->HandleMessage(this, Hwnd, Msg, WParam, LParam);
//	}
//}

void FCore::Release()
{
	//if (MainViewportClient && GRenderer)
	//{
	//	MainViewportClient->Detach(this, GRenderer);
	//}
	//MainViewportClient = nullptr;

	if (LevelManager)
	{
		LevelManager->Release();
		LevelManager.reset();
	}

	if (ObjManager)
	{
		ObjManager->FlushKilledObjects();
		delete ObjManager;
		ObjManager = nullptr;
	}

	//delete EnhancedInput;
	//EnhancedInput = nullptr;
	//delete InputManager;
	//InputManager = nullptr;
	CPrimitiveBase::ClearCache();

	if (GRenderer)
	{
		delete GRenderer;
		GRenderer = nullptr;
	}
}

void FCore::Tick()
{
	Timer.Tick();
	Tick(Timer.GetDeltaTime());
}

void FCore::Tick(const float DeltaTime)
{
	Input(DeltaTime);
	Physics(DeltaTime);
	GameLogic(DeltaTime);
	//Render();
	LateUpdate(DeltaTime);
}

//void FCore::RenderViewport(TArray<AActor*>& Actors, FViewportContext& ViewportContext)
//{
//	FViewportClient* Client = ViewportContext.GetViewportClient();
//	FViewport* Viewport = ViewportContext.GetViewport();
//	if (!ViewportContext.GetViewport() && ViewportContext.GetViewportClient())
//	{
//		return;
//	}
//
//	CommandQueue.Clear();
//	CommandQueue.Reserve(GRenderer->GetPrevCommandCount());
//
//	ViewportContext.GetViewportClient()->BuildRenderCommands(Actors, CommandQueue);
//
//	Renderer->SetViewport(&ViewportContext.GetViewport()->GetD3D11Viewport());
//
//	Renderer->SubmitCommands(CommandQueue);
//	Renderer->ExecuteCommands();
//}

void FCore::Input(float DeltaTime)
{
	(void)DeltaTime;
}

void FCore::Physics(float DeltaTime)
{
	ULevel* Level = LevelManager.get()->GetActiveLevel();
	if (Level)
	{
		FVector LineStart(2, 2, 0), LineEnd(5, 5, 0);
		FHitResult HitResult;

		bool bHit = PhysicsManager->Linetrace(Level, LineStart, LineEnd, HitResult);

		if (bHit)
		{
			if (!HitResult.HitActor->IsA(ASkySphereActor::StaticClass()))
			{
				for (UActorComponent* ActorComp : HitResult.HitActor->GetComponents())
				{
					if (!ActorComp->IsA(UPrimitiveComponent::StaticClass()))
					{
						continue;
					}

					UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(ActorComp);
					if (!PrimComp->ShouldDrawDebugBounds())
					{
						continue;
					}

					FBoxSphereBounds Bound = PrimComp->GetWorldBounds();
					DebugDrawManager.DrawCube(Bound.Center, Bound.BoxExtent, FVector4(1, 0, 0, 1));
				}

				DebugDrawManager.DrawCube(HitResult.HitLocation, FVector(0.1, 0.1, 0.1), FVector4(0, 1, 0, 1));
			}
		}

		if (GRenderer)
		{
			DebugDrawManager.DrawLine(LineStart, LineEnd, FVector4(0, 1, 1, 1));
		}
	}
}

void FCore::GameLogic(float DeltaTime)
{
	UWorld* World = GetActiveWorld();
	if (World)
	{
		World->Tick(DeltaTime);
	}
}

void FCore::LateUpdate(float DeltaTime)
{
	if (GCInterval <= 0.0)
	{
		return;
	}

	double CurrentTime = Timer.GetTotalTime();
	if (ObjManager && (CurrentTime - LastGCTime) >= GCInterval)
	{
		ObjManager->FlushKilledObjects();
		LastGCTime = CurrentTime;
	}
}

//void FCore::Render()
//{
//	ULevel* Level = MainViewportClient ? MainViewportClient->ResolveLevel(this) : GetActiveLevel();
//	if (!Renderer || !Level || Renderer->IsOccluded())
//	{
//		return;
//	}
//
//	Renderer->BeginFrame();
//
//	UWorld* ActiveWorld = GetActiveWorld();
//	if (!ActiveWorld)
//	{
//		Renderer->EndFrame();
//		return;
//	}
//
//	for (FViewportClient* Client : ViewportClients)
//	{
//		if (!Client)
//		{
//			continue;
//		}
//
//		CommandQueue.Clear();
//		CommandQueue.Reserve(Renderer->GetPrevCommandCount());
//
//		TArray<AActor*> Actors = ActiveWorld->GetAllActors();
//		Client->BuildRenderCommands(Actors, CommandQueue);
//
//		D3D11_VIEWPORT D3D11Viewport = {};
//		D3D11Viewport.TopLeftX = static_cast<float>(Client->TopLeftX);
//		D3D11Viewport.TopLeftY = static_cast<float>(Client->TopLeftY);
//		D3D11Viewport.Width = static_cast<float>(Client->Width);
//		D3D11Viewport.Height = static_cast<float>(Client->Height);
//		D3D11Viewport.MaxDepth = 1.0f;
//		D3D11Viewport.MinDepth = 0.0f;
//		Renderer->SetViewport(&D3D11Viewport);
//
//		Renderer->SubmitCommands(CommandQueue);
//		Renderer->ExecuteCommands();
//		const FShowFlags& ShowFlags = Client->GetShowFlags();
//		DebugDrawManager.Flush(Renderer.get(), ShowFlags, ActiveWorld);
//	}
//
//	Renderer->EndFrame();
//}

void FCore::OnResize(int32 Width, int32 Height)
{
	if (Width == 0 || Height == 0)
	{
		return;
	}

	WindowWidth = Width;
	WindowHeight = Height;
	if (GRenderer)
	{
		GRenderer->OnResize(Width, Height);
	}
	if (LevelManager)
	{
		LevelManager->OnResize(Width, Height);
	}
}

void FCore::RegisterConsoleVariables()
{
	FConsoleVariableManager& CVM = FConsoleVariableManager::Get();

	FConsoleVariable* MaxFPSVar = CVM.Find("t.MaxFPS");
	if (!MaxFPSVar)
	{
		MaxFPSVar = CVM.Register("t.MaxFPS", 0.0f, "Maximum FPS limit (0 = unlimited)");
	}
	MaxFPSVar->SetOnChanged([this](FConsoleVariable* Var)
		{
			Timer.SetMaxFPS(Var->GetFloat());
		});
	Timer.SetMaxFPS(MaxFPSVar->GetFloat());

	FConsoleVariable* VSyncVar = CVM.Find("r.VSync");
	if (!VSyncVar)
	{
		VSyncVar = CVM.Register("r.VSync", 0, "Enable VSync (0 = off, 1 = on)");
	}
	VSyncVar->SetOnChanged([this](FConsoleVariable* Var)
		{
			if (GRenderer)
			{
				GRenderer->SetVSync(Var->GetInt() != 0);
			}
		});
	if (GRenderer)
	{
		GRenderer->SetVSync(VSyncVar->GetInt() != 0);
	}

	FConsoleVariable* GCIntervalVar = CVM.Find("gc.Interval");
	if (!GCIntervalVar)
	{
		GCIntervalVar = CVM.Register("gc.Interval", 30.0f, "GC interval in seconds (0 = disabled)");
	}
	GCIntervalVar->SetOnChanged([this](FConsoleVariable* Var)
		{
			GCInterval = static_cast<double>(Var->GetFloat());
		});
	GCInterval = static_cast<double>(GCIntervalVar->GetFloat());

	CVM.RegisterCommand("ForceGC", [this](FString& OutResult)
		{
			if (ObjManager)
			{
				ObjManager->FlushKilledObjects();
				LastGCTime = Timer.GetTotalTime();
				OutResult = "ForceGC: Garbage collection completed.";
			}
			else
			{
				OutResult = "ForceGC: ObjectManager is not available.";
			}
		}, "Force immediate garbage collection");
}
