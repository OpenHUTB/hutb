// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "CarlaUE4.h"
#include "EasyFileManager.h"

EasyFileManager::EasyFileManager()
{
}

EasyFileManager::~EasyFileManager()
{
}

void EasyFileManager::SaveToFile(FString Data, FString FileName, FString Path, bool bOverwrite)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.CreateDirectoryTree(*Path))
	{
		FString AbsoluteFilePath = Path + FileName;
		if (bOverwrite || !PlatformFile.FileExists(*AbsoluteFilePath))
		{
			FFileHelper::SaveStringToFile(Data, *AbsoluteFilePath);
		}
	}
}
