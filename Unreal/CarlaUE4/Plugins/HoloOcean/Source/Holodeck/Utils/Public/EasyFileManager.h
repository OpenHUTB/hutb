// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

/**
 * 
 */
class HOLODECK_API EasyFileManager
{
public:
	EasyFileManager();
	~EasyFileManager();

	void SaveToFile(FString Data, FString FileName, FString Path = "C:\\Users\\robert.pottorff\\Desktop\\", bool bOverwrite = true);
};
