// Copyright (c) 2026 Jonathan Embley-Riches. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// --- LEGAL DISCLAIMER ---
// UnrealRoboticsLab is an independent software plugin. It is NOT affiliated with,
// endorsed by, or sponsored by Epic Games, Inc. "Unreal" and "Unreal Engine" are
// trademarks or registered trademarks of Epic Games, Inc. in the US and elsewhere.
//
// This plugin incorporates third-party software: MuJoCo (Apache 2.0),
// CoACD (MIT), and libzmq (MPL 2.0). See ThirdPartyNotices.txt for details.

#pragma once

#include "CoreMinimal.h"

/**
 * @class FMjPythonHelper
 * @brief Manages Python interpreter resolution, package checking, and install prompts.
 *
 * Default: uses UE's bundled Python (Engine/Binaries/ThirdParty/Python3/).
 * Override: user can specify a custom path stored in Config/LocalUnrealRoboticsLab.ini.
 */
class FMjPythonHelper
{
public:
	/**
	 * @brief Resolve the Python interpreter to use.
	 * Checks LocalUnrealRoboticsLab.ini for a user override, otherwise returns UE's bundled Python.
	 * @return Path to the python executable, or empty string if none found.
	 */
	static FString ResolvePythonPath();

	/** @brief Get UE's bundled Python path. */
	static FString GetUEBundledPythonPath();

	/** @brief Read user's custom Python override from local config. */
	static FString GetStoredPythonOverride();

	/** @brief Store a custom Python path to local config. */
	static void StorePythonOverride(const FString& PythonPath);

	/** @brief Validate a Python binary by running --version. */
	static bool ValidatePythonBinary(const FString& PythonPath);

	/** @brief Check if trimesh and numpy are importable. */
	static bool CheckPythonPackages(const FString& PythonPath);

	/** @brief Run pip install trimesh numpy. Returns true on success. */
	static bool InstallPythonPackages(const FString& PythonPath, FString& OutLog);

	/**
	 * @brief Ensure Python is available with required packages.
	 * Shows dialogs as needed. Returns the resolved Python path, or empty if
	 * the user skipped preprocessing or cancelled the import.
	 * @param bOutCancelled Set to true if the user chose to cancel the import entirely.
	 */
	static FString EnsurePythonReady(bool& bOutCancelled);

private:
	static FString GetLocalIniPath();

	/** Opens a file picker and validates the selected Python binary. Returns empty if cancelled or invalid. */
	static FString BrowseForPython();
};
