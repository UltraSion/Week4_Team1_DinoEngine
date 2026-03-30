#include "Paths.h"
#include <filesystem>
#include <Windows.h>

namespace fs = std::filesystem;

// FString FPaths::Root;
std::filesystem::path FPaths::Root;
bool FPaths::bInitialized = false;

void FPaths::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	// 실행 파일 경로 획득
	wchar_t ExePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, ExePath, MAX_PATH);

	fs::path ExeDir = fs::path(ExePath).parent_path();

	// 실행 파일은 항상 {Root}/{Project}/Bin/{Config}/ 에 위치
	// 3단계 상위 = 프로젝트 루트
	fs::path Candidate = ExeDir.parent_path().parent_path().parent_path();

	// 검증: Engine/ 과 Assets/ 디렉토리가 모두 존재하는지 확인
	if (fs::is_directory(Candidate / "Engine") && fs::is_directory(Candidate / "Assets"))
	{
		SetRoot(Candidate);
		return;
	}

	// 폴백: 상위 디렉토리를 순회하며 Engine/ + Assets/ 조합 탐색
	fs::path CurrentDir = ExeDir;
	for (int32 i = 0; i < 10; ++i)
	{
		if (fs::is_directory(CurrentDir / "Engine") && fs::is_directory(CurrentDir / "Assets"))
		{
			SetRoot(CurrentDir);
			return;
		}
		CurrentDir = CurrentDir.parent_path();
	}

	// 최종 폴백: 실행 파일 디렉토리 사용
	SetRoot(ExeDir);
}
//단순히 바이트 복사만 하던 방식을 버리고 Windows API를 이용해 완벽한 UTF-8 -> UTF-16 변환 수행
std::wstring FPaths::ToWide(const FString& Path)
{
	if (Path.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &Path[0], (int)Path.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &Path[0], (int)Path.size(), &wstrTo[0], size_needed);
	return wstrTo;
}
//fs::path.string()을 쓰지 않고 u8string()을 거쳐 안전하게 UTF-8로 추출 및 조작
std::string FPaths::ToRelativePath(const FString& Path)
{
	auto u8Root = ProjectRoot().u8string();
	FString Root(u8Root.begin(), u8Root.end());
	FString RelativePath = Path;

	if (RelativePath.starts_with(Root))
	{
		RelativePath = RelativePath.substr(Root.length());
		// 앞에 슬래시가 남아있다면 제거 (예: "/Assets/..." -> "Assets/...")
		if (!RelativePath.empty() && (RelativePath.front() == '/' || RelativePath.front() == '\\'))
		{
			RelativePath = RelativePath.substr(1);
		}
	}

	return RelativePath;
}
//fs::path 생성자를 거치지 않고 순수 문자열 결합을 사용하여 인코딩 크래시 원천 차단
std::string FPaths::ToAbsolutePath(const FString& Path)
{
	auto u8Root = ProjectRoot().u8string();
	FString Root(u8Root.begin(), u8Root.end());

	// 이미 절대 경로라면 그대로 반환
	if (Path.starts_with(Root))
	{
		return Path;
	}

	FString AbsolutePath = Root;
	// Root 끝에 슬래시가 없으면 추가
	if (!AbsolutePath.empty() && AbsolutePath.back() != '/' && AbsolutePath.back() != '\\')
	{
		AbsolutePath += "/";
	}

	// Path 시작에 슬래시가 있으면 제거하여 중복 방지
	FString SafePath = Path;
	if (!SafePath.empty() && (SafePath.front() == '/' || SafePath.front() == '\\'))
	{
		SafePath = SafePath.substr(1);
	}

	AbsolutePath += SafePath;
	return AbsolutePath;
}

void FPaths::SetRoot(const std::filesystem::path& InPath)
{
	Root = InPath;
	bInitialized = true;
}

const std::filesystem::path& FPaths::ProjectRoot()
{
	return Root;
}

std::filesystem::path FPaths::EngineDir()
{
	return Root / "Engine/";
}

std::filesystem::path FPaths::ShaderDir()
{
	return Root / "Engine/Shaders/";
}

std::filesystem::path FPaths::AssetDir()
{
	return Root / "Assets/";
}

std::filesystem::path FPaths::LevelDir()
{
	return Root / "Assets/Levels/";
}

std::filesystem::path FPaths::MaterialDir()
{
	return Root / "Assets/Materials/";
}

std::filesystem::path FPaths::MeshDir()
{
	return Root / "Assets/Meshes/";
}

std::filesystem::path FPaths::ContentDir()
{
	return Root / "Content/";
}

std::filesystem::path FPaths::ShaderCacheDir()
{
	return Root / "Content/Shaders/";
}

std::filesystem::path FPaths::IntermediateDir()
{
	return Root / "Intermediate/";
}

/*
FString FPaths::Combine(const FString& Base, const FString& Relative)
{
	if (Base.empty())
	{
		return Relative;
	}
	if (Base.back() == '/' || Base.back() == '\\')
	{
		return Base + Relative;
	}
	return Base + "/" + Relative;
}

std::wstring FPaths::ToWide(const FString& Path)
{
	return std::wstring(Path.begin(), Path.end());
}
*/