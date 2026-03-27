#pragma once

#include "Actor.h"

class UStaticMeshComp;
struct ID3D11Device;

class ENGINE_API AStaticMeshActor : public AActor
{
public:
	DECLARE_RTTI(AStaticMeshActor, AActor)
};