// Copyright Epic Games, Inc. All Rights Reserved.


using System;
using System.Diagnostics;
using UnrealBuildTool;
using System.IO;
using System.Text.RegularExpressions;

public class URLab : ModuleRules
{
	public URLab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		//bUseRTTI = true;  // Required for type info
		//bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"PhysicsCore",
			"XmlParser",
			"HTTP",
			"Json",
			"JsonUtilities",
			"Slate",
			"SlateCore",
			"Projects",
			"ApplicationCore",
			"RenderCore",
			"RHI",
			"UMG",
			"AssetRegistry",
			"ProceduralMeshComponent",
			"GeometryFramework",
			"GeometryCore"
		});

		// Editor-only dependencies for DecomposeMesh and other #if WITH_EDITOR code
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"AssetTools"
			});
		}

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CinematicCamera",
			"ImageWrapper",
			"EnhancedInput",
			"Chaos",
			"Landscape",
			"Eigen",
			"DesktopPlatform"
		});

		DynamicallyLoadedModuleNames.AddRange(new string[]
		{
			// Add dynamically loaded modules here
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"kernel32.lib",
				"user32.lib"
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicSystemLibraries.AddRange(new string[]
			{
				"pthread", "dl", "rt"
			});
		}

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// Ensure Unreal recognizes MuJoCo's export macros
		}


		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicDefinitions.Add("_WIN32=0");
			PublicDefinitions.Add("USE_DECLSPEC=1");
			PublicDefinitions.Add("__linux__=1");
			PublicDefinitions.Add("__unix__=1");
		}


		VerifyThirdPartyInstalls();

		AddMuj(Target);
		AddCoACD(Target);
		AddZeroMQ(Target);
	}

	private string ThirdPartyPath
	{
		get { return Path.Combine(PluginDirectory, "third_party", "install"); }
	}

	private void AddThirdPartyLibrary(string LibraryName, ReadOnlyTargetRules Target)
	{
		string FullPath = Path.Combine(ThirdPartyPath, LibraryName);
		
		// Add include directory if it exists
		string IncludePath = Path.Combine(FullPath, "include");
		if (Directory.Exists(IncludePath))
		{
			PublicIncludePaths.Add(IncludePath);
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Link all libraries in the lib directory
			string LibPath = Path.Combine(FullPath, "lib");
			if (Directory.Exists(LibPath))
			{
				string[] LibFiles = Directory.GetFiles(LibPath, "*.lib", SearchOption.AllDirectories);
				foreach (string LibFile in LibFiles)
				{
					PublicAdditionalLibraries.Add(LibFile);
				}
			}

			// Stage DLLs next to executable for packaged builds
			// Skip MSVC runtime DLLs (handled by redistributable installer)
			string BinPath = Path.Combine(FullPath, "bin");
			if (Directory.Exists(BinPath))
			{
				string[] DllFiles = Directory.GetFiles(BinPath, "*.dll", SearchOption.AllDirectories);
				foreach (string DllFile in DllFiles)
				{
					string DllName = Path.GetFileName(DllFile);
					if (DllName.StartsWith("vcruntime") || DllName.StartsWith("msvcp") || DllName.StartsWith("concrt"))
						continue;
					RuntimeDependencies.Add("$(BinaryOutputDir)/" + DllName, DllFile, StagedFileType.NonUFS);
					PublicDelayLoadDLLs.Add(DllName);
				}
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			// Link all static libraries and shared objects for Linux
			string LibPath = Path.Combine(FullPath, "lib");
			if (Directory.Exists(LibPath))
			{
				foreach (string LibFile in Directory.GetFiles(LibPath, "*.a", SearchOption.AllDirectories))
				{
					PublicAdditionalLibraries.Add(LibFile);
				}
				foreach (string LibFile in Directory.GetFiles(LibPath, "*.so", SearchOption.AllDirectories))
				{
					PublicAdditionalLibraries.Add(LibFile);
					RuntimeDependencies.Add(LibFile);
					PublicDelayLoadDLLs.Add(Path.GetFileName(LibFile));
				}
			}

			// Add binaries (often plugin/shared objects) for Linux
			string BinPath = Path.Combine(FullPath, "bin");
			if (Directory.Exists(BinPath))
			{
				foreach (string BinFile in Directory.GetFiles(BinPath, "*.so", SearchOption.AllDirectories))
				{
					RuntimeDependencies.Add(BinFile);
					PublicDelayLoadDLLs.Add(Path.GetFileName(BinFile));
					PublicAdditionalLibraries.Add(BinFile);
				}
			}
		}
	}

	// Set to true to skip the submodule/install SHA drift checks. The default
	// (false) catches two common failure modes before any code compiles:
	//   A) "I git-pulled URLab but forgot `git submodule update`" - the pinned
	//      submodule SHA has moved but the working tree still points at the old
	//      one, so we'd build against stale MuJoCo/CoACD/libzmq source.
	//   B) "I updated submodules but forgot to re-run third_party/*/build.ps1" -
	//      the source is fresh but third_party/install/<dep>/ was built from an
	//      older SHA, so UE links against a mismatched binary.
	// Flip this to true if you are intentionally iterating on a submodule
	// locally. The per-dep build scripts accept -NoSubmoduleSync / --no-submodule-sync
	// for the same purpose at build-script time.
	// Static readonly (not const) so toggling to `true` doesn't produce
	// CS0162 "unreachable code" warnings in the active branch.
	private static readonly bool SkipThirdPartyDriftChecks = false;

	// Names match the directories under third_party/ and third_party/install/.
	private static readonly string[] ThirdPartyPackages = new[] { "MuJoCo", "CoACD", "libzmq" };

	protected void AddMuj(ReadOnlyTargetRules Target)
	{
		AddThirdPartyLibrary("MuJoCo", Target);
	}

	// Checks each third-party dep for submodule drift (Layer A) and install
	// drift (Layer B). Runs once up front so any failure message is clearly
	// attributable before UBT tries to resolve libs/includes. If URLab itself
	// isn't a git checkout (release zip), the checks self-disable with a note.
	private void VerifyThirdPartyInstalls()
	{
		if (SkipThirdPartyDriftChecks)
		{
			Console.WriteLine("URLab: SkipThirdPartyDriftChecks=true - skipping submodule/install SHA checks.");
			return;
		}

		string PluginRoot = PluginDirectory;
		bool IsGitCheckout = Directory.Exists(Path.Combine(PluginRoot, ".git"))
			|| File.Exists(Path.Combine(PluginRoot, ".git"));
		if (!IsGitCheckout)
		{
			Console.WriteLine("URLab: plugin root is not a git checkout - skipping submodule/install SHA checks.");
			return;
		}

		RegisterDriftCheckDependencies(PluginRoot);

		foreach (string Package in ThirdPartyPackages)
		{
			VerifyPackageInstall(PluginRoot, Package);
		}
	}

	// Tell UBT to re-evaluate this module when any of these files change so
	// the Layer A/B drift checks fire on the next build. Without this, UBT's
	// makefile cache short-circuits with "Target is up to date" and Build.cs
	// never runs, letting a stale install slip through.
	private void RegisterDriftCheckDependencies(string PluginRoot)
	{
		// URLab's own index - covers `git pull` / `git reset` on the plugin,
		// which is how an out-of-sync submodule pointer typically appears.
		string PluginGitIndex = Path.Combine(PluginRoot, ".git", "index");
		if (File.Exists(PluginGitIndex))
		{
			ExternalDependencies.Add(PluginGitIndex);
		}

		foreach (string Package in ThirdPartyPackages)
		{
			// Submodule HEAD file - Git rewrites this on checkout/reset/update,
			// so it's the right signal for "submodule HEAD moved" (Layer A).
			string SubmoduleGitHead = Path.Combine(PluginRoot, ".git", "modules",
				"third_party", Package, "src", "HEAD");
			if (File.Exists(SubmoduleGitHead))
			{
				ExternalDependencies.Add(SubmoduleGitHead);
			}

			// INSTALLED_SHA.txt - changes whenever build.ps1 re-installs, or
			// (as in our tests) when edited by hand. Covers Layer B.
			string ShaFile = Path.Combine(ThirdPartyPath, Package, "INSTALLED_SHA.txt");
			if (File.Exists(ShaFile))
			{
				ExternalDependencies.Add(ShaFile);
			}
		}
	}

	private void VerifyPackageInstall(string PluginRoot, string Package)
	{
		string SubmoduleRelPath = "third_party/" + Package + "/src";
		string SubmoduleAbsPath = Path.Combine(PluginRoot, "third_party", Package, "src");
		string InstallDir = Path.Combine(ThirdPartyPath, Package);
		string ShaFile = Path.Combine(InstallDir, "INSTALLED_SHA.txt");

		// Submodule not initialised yet: directory missing, or present but empty.
		bool SubmoduleMissing = !Directory.Exists(SubmoduleAbsPath)
			|| Directory.GetFileSystemEntries(SubmoduleAbsPath).Length == 0;
		if (SubmoduleMissing)
		{
			throw new BuildException(
				"{0} submodule not initialised at '{1}'. From the plugin root run:\n" +
				"  git submodule update --init --recursive --force\n" +
				"then re-run third_party/{0}/build.ps1 (Windows) or build.sh (Linux/macOS). " +
				"To bypass this check, set SkipThirdPartyDriftChecks=true in URLab.Build.cs.",
				Package, SubmoduleAbsPath);
		}

		// LAYER A: submodule HEAD vs URLab's committed pointer.
		// Catches "pulled URLab but forgot `git submodule update`".
		string ExpectedSha;
		try
		{
			string Pointer = RunGit(PluginRoot, "ls-tree HEAD " + SubmoduleRelPath);
			ExpectedSha = ParseSubmodulePointerSha(Pointer, Package);
		}
		catch (Exception Ex)
		{
			// Shallow clones or repos without a HEAD pointing at the submodule
			// (edge cases) can't run Layer A. Warn and continue to Layer B.
			Console.WriteLine("URLab: could not read URLab pointer for {0} ({1}) - skipping Layer A check.",
				Package, Ex.Message);
			ExpectedSha = null;
		}

		string ActualSha;
		try
		{
			ActualSha = RunGit(SubmoduleAbsPath, "rev-parse HEAD");
		}
		catch (Exception Ex)
		{
			throw new BuildException(
				"{0} submodule at '{1}' is not a valid git checkout ({2}). From the plugin root run:\n" +
				"  git submodule update --init --recursive --force\n" +
				"then re-run third_party/{0}/build.ps1.",
				Package, SubmoduleAbsPath, Ex.Message);
		}

		if (ExpectedSha != null && !string.Equals(ExpectedSha, ActualSha, StringComparison.OrdinalIgnoreCase))
		{
			throw new BuildException(
				"{0} submodule drift: URLab expects SHA {1} but third_party/{0}/src/ is at {2}. " +
				"From the plugin root run:\n" +
				"  git submodule update --init --recursive --force\n" +
				"then re-run third_party/{0}/build.ps1 (Windows) or build.sh (Linux/macOS). " +
				"To bypass this check, set SkipThirdPartyDriftChecks=true in URLab.Build.cs.",
				Package, ExpectedSha, ActualSha);
		}

		// LAYER B: install SHA (what build.ps1 recorded) vs submodule HEAD.
		// Catches "updated submodules but forgot to re-run build.ps1".
		if (!File.Exists(ShaFile))
		{
			throw new BuildException(
				"{0} install is missing '{1}'. Run third_party/{0}/build.ps1 (Windows) or " +
				"third_party/{0}/build.sh (Linux/macOS). " +
				"To bypass this check, set SkipThirdPartyDriftChecks=true in URLab.Build.cs.",
				Package, ShaFile);
		}

		string InstalledSha = File.ReadAllText(ShaFile).Trim();
		if (!string.Equals(InstalledSha, ActualSha, StringComparison.OrdinalIgnoreCase))
		{
			throw new BuildException(
				"{0} install is stale: built from SHA {1} but third_party/{0}/src/ is now at {2}. " +
				"Re-run third_party/{0}/build.ps1 (Windows) or third_party/{0}/build.sh (Linux/macOS). " +
				"To bypass this check, set SkipThirdPartyDriftChecks=true in URLab.Build.cs.",
				Package, InstalledSha, ActualSha);
		}
	}

	private static string RunGit(string WorkingDir, string Arguments)
	{
		ProcessStartInfo PSI = new ProcessStartInfo("git", Arguments)
		{
			WorkingDirectory = WorkingDir,
			RedirectStandardOutput = true,
			RedirectStandardError = true,
			UseShellExecute = false,
			CreateNoWindow = true,
		};
		using (Process Proc = Process.Start(PSI))
		{
			string Stdout = Proc.StandardOutput.ReadToEnd();
			string Stderr = Proc.StandardError.ReadToEnd();
			Proc.WaitForExit();
			if (Proc.ExitCode != 0)
			{
				throw new Exception(string.Format("git {0} exited {1}: {2}",
					Arguments, Proc.ExitCode, Stderr.Trim()));
			}
			return Stdout.Trim();
		}
	}

	private static string ParseSubmodulePointerSha(string Output, string Package)
	{
		// `git ls-tree HEAD <path>` output: "160000 commit <SHA>\t<path>"
		Match M = Regex.Match(Output, @"160000\s+commit\s+([0-9a-f]{40})");
		if (!M.Success)
		{
			throw new Exception(string.Format(
				"could not parse submodule pointer for {0}: '{1}'",
				Package, Output));
		}
		return M.Groups[1].Value;
	}

	protected void AddCoACD(ReadOnlyTargetRules Target)
	{
		AddThirdPartyLibrary("CoACD", Target);
		PublicDefinitions.Add("COACD_EXPORTS=1");
	}

	protected void AddZeroMQ(ReadOnlyTargetRules Target)
	{
		AddThirdPartyLibrary("libzmq", Target);
	}
}
