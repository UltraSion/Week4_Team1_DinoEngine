#include "Core.h"

#include "Actor/Actor.h"
#include "Actor/SkySphereActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/UUIDBillboardComponent.h"
#include "Core/ConsoleVariableManager.h"
#include "Core/Paths.h"
#include "Input/EnhancedInputManager.h"
#include "Input/InputManager.h"
#include "Math/Frustum.h"
#include "Mesh/ObjManager.h"
#include "Object/ObjectFactory.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectManager.h"
#include "Primitive/PrimitiveBase.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Renderer.h"
#include "ViewportClient.h"
#include "World/Level.h"

#include <filesystem>
#include "Mesh/ObjManager.h"
#include "Asset/AssetManager.h"
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


	FString AssetRootDir = (FPaths::ProjectRoot() / "Assets").string();
	FAssetManager::Get().Initialize(AssetRootDir);
	// Material
	FMaterialManager::Get().LoadAllMaterials(GRenderer->GetDevice(), GRenderer->GetRenderStateManager().get());
	{
		namespace fs = std::filesystem;
		const fs::path MeshDir = FPaths::ProjectRoot() / "Assets" / "Meshes";
		if (fs::exists(MeshDir))
		{
			for (const auto& Entry : fs::directory_iterator(MeshDir))
			{
				if (Entry.is_regular_file() && Entry.path().extension() == ".obj")
				{
					const auto RelativePath = fs::relative(Entry.path(), FPaths::ProjectRoot());
					FObjManager::LoadObjStaticMesh(RelativePath.generic_string());
				}
			}
		}
	}

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

void FCore::Release()
{
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
	LateUpdate(DeltaTime);
}

void FCore::Input(float DeltaTime)
{
	(void)DeltaTime;
}

void FCore::Physics(float DeltaTime)
{
	(void)DeltaTime;

	ULevel* Level = LevelManager ? LevelManager->GetActiveLevel() : nullptr;
	if (!Level)
	{
		return;
	}

	FVector LineStart(2, 2, 0);
	FVector LineEnd(5, 5, 0);
	FHitResult HitResult;

	if (PhysicsManager->Linetrace(Level, LineStart, LineEnd, HitResult))
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

				const FBoxSphereBounds Bounds = PrimComp->GetWorldBounds();
				DebugDrawManager.DrawCube(Bounds.Center, Bounds.BoxExtent, FVector4(1, 0, 0, 1));
			}

			DebugDrawManager.DrawCube(HitResult.HitLocation, FVector(0.1, 0.1, 0.1), FVector4(0, 1, 0, 1));
		}
	}

	if (GRenderer)
	{
		DebugDrawManager.DrawLine(LineStart, LineEnd, FVector4(0, 1, 1, 1));
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
	(void)DeltaTime;

	if (GCInterval <= 0.0)
	{
		return;
	}

	const double CurrentTime = Timer.GetTotalTime();
	if (ObjManager && (CurrentTime - LastGCTime) >= GCInterval)
	{
		ObjManager->FlushKilledObjects();
		LastGCTime = CurrentTime;
	}
}

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
