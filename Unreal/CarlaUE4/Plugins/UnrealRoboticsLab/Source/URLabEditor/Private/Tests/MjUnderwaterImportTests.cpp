// OpenHUTB 追加（2026-11-04）
//
// 验证水下机器人 ROV 模型能否通过 URLab 的 MJCF 导入管线正确导入。
// 目的：证实「导入器不支持 .stl，导致 mesh 退化为立方体」这一判断。
//
// 用法：
//   UE4Editor.exe <proj> -ExecCmds="Automation RunTests URLab.Import.UnderwaterRov; Quit" -unattended
//
// 通过环境变量 ROV_XML_PATH 指定要导入的 XML，默认用修复后的 OBJ 版本：
//   set ROV_XML_PATH=...\rov_model_fixed\underwater_rov_with_arm.xml   (期望：通过)
//   set ROV_XML_PATH=...\models\underwater_rov_with_arm.xml            (期望：mesh 缺失)
//
// 参考 docs/notes/ROV模型导入问题定位.md

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"

#include "Tests/MjTestHelpers.h"
#include "MuJoCo/Components/Geometry/MjGeom.h"
#include "MuJoCo/Components/Bodies/MjBody.h"
#include "MuJoCo/Components/Joints/MjJoint.h"
#include "MuJoCo/Components/Actuators/MjActuator.h"

#include "mujoco/mujoco.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMjImportUnderwaterRov,
	"URLab.Import.UnderwaterRov",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMjImportUnderwaterRov::RunTest(const FString& Parameters)
{
	// 允许通过环境变量切换待测模型，便于对比 STL 与 OBJ 两个版本
	FString XmlPath = FPlatformMisc::GetEnvironmentVariable(TEXT("ROV_XML_PATH"));
	if (XmlPath.IsEmpty())
	{
		XmlPath = TEXT("E:/Projects/underwater/out/rov_model_fixed/underwater_rov_with_arm.xml");
	}

	AddInfo(FString::Printf(TEXT("Importing: %s"), *XmlPath));

	if (!FPaths::FileExists(XmlPath))
	{
		AddWarning(FString::Printf(TEXT("跳过：XML 不存在 %s"), *XmlPath));
		return true;
	}

	FMjXmlImportSession S;
	if (!S.InitFromFile(XmlPath))
	{
		AddError(FString::Printf(TEXT("InitFromFile 失败：%s"), *S.LastError));
		S.Cleanup();
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("原生 MuJoCo 基准：bodies=%d joints=%d actuators=%d geoms=%d"),
		(int32)S.NativeBodyCount, (int32)S.NativeJointCount,
		(int32)S.NativeActuatorCount, (int32)S.NativeGeomCount));

	const int32 BPBodies = S.CountTemplates<UMjBody>();
	const int32 BPJoints = S.CountTemplates<UMjJoint>();
	const int32 BPActuators = S.CountTemplates<UMjActuator>();
	const int32 BPGeoms = S.CountTemplates<UMjGeom>();

	AddInfo(FString::Printf(
		TEXT("蓝图组件：bodies=%d joints=%d actuators=%d geoms=%d（含 defaults）"),
		BPBodies, BPJoints, BPActuators, BPGeoms));

	TestTrue(TEXT("蓝图应有 body 组件"), BPBodies >= 1);
	TestTrue(TEXT("蓝图应有 geom 组件"), BPGeoms >= 1);

	if (!S.Compile())
	{
		AddError(FString::Printf(TEXT("Compile 失败：%s"), *S.LastError));
		S.Cleanup();
		return false;
	}

	mjModel* M = S.Model();
	if (M == nullptr)
	{
		AddError(TEXT("编译后模型为空"));
		S.Cleanup();
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("URLab 编译结果：bodies=%d joints=%d actuators=%d geoms=%d meshes=%d"),
		(int32)M->nbody, (int32)M->njnt, (int32)M->nu, (int32)M->ngeom, (int32)M->nmesh));

	// 结构一致性：应与原生 MuJoCo 一致
	TestEqual(TEXT("body 数应与原生一致"), (int32)M->nbody, (int32)S.NativeBodyCount);
	TestEqual(TEXT("joint 数应与原生一致"), (int32)M->njnt, (int32)S.NativeJointCount);
	TestEqual(TEXT("actuator 数应与原生一致"), (int32)M->nu, (int32)S.NativeActuatorCount);
	TestEqual(TEXT("geom 数应与原生一致"), (int32)M->ngeom, (int32)S.NativeGeomCount);

	// 关键判据：mesh 是否被正确加载
	// ROV 模型有 28 个 mesh；若导入器无法解析文件，nmesh 会明显小于 28
	AddInfo(FString::Printf(TEXT("mesh 数量：%d（期望 28）"), (int32)M->nmesh));
	TestTrue(TEXT("应加载到 mesh（期望 28 个）"), (int32)M->nmesh >= 20);

	// 能步进即说明模型可用
	mjData* D = S.Data();
	if (D != nullptr)
	{
		mj_step(M, D);
		mj_step(M, D);
		AddInfo(TEXT("模型可正常步进 2 次"));

		// ------------------------------------------------------------------
		// 抓取物理连通性验证
		//   驱动夹爪执行器闭合，步进若干帧，确认：
		//     1) 夹爪指关节确实在动（qpos 变化）
		//     2) 场景中出现接触（说明碰撞体在工作）
		//   注意：模型本身不含可抓取试件，这里只验证物理链路，不做抓取成功率统计。
		// ------------------------------------------------------------------
		// URLab 会给 spec 里的元素名加蓝图前缀（ImportTestArt_xxx_），
		// 所以不能按原名查找，改为后缀匹配。
		auto FindByNameSuffix = [](const mjModel* Model, mjtObj Type, const FString& Suffix) -> int32
		{
			const int32 Count = (Type == mjOBJ_ACTUATOR) ? (int32)Model->nu
				: (Type == mjOBJ_JOINT) ? (int32)Model->njnt : 0;
			for (int32 i = 0; i < Count; ++i)
			{
				const char* Name = mj_id2name(Model, Type, i);
				if (Name && FString(ANSI_TO_TCHAR(Name)).EndsWith(Suffix))
				{
					return i;
				}
			}
			return -1;
		};

		const int32 ActId = FindByNameSuffix(M, mjOBJ_ACTUATOR, TEXT("gripper_fingers_actuator"));
		if (ActId >= 0)
		{
			// 记录初始指关节位置
			const int32 JntL = FindByNameSuffix(M, mjOBJ_JOINT, TEXT("gripper_left_driver_joint"));
			const int32 JntR = FindByNameSuffix(M, mjOBJ_JOINT, TEXT("gripper_right_driver_joint"));
			const double Q0L = (JntL >= 0) ? D->qpos[M->jnt_qposadr[JntL]] : 0.0;
			const double Q0R = (JntR >= 0) ? D->qpos[M->jnt_qposadr[JntR]] : 0.0;

			// 闭合夹爪（ctrlrange 是 0..255，取上限）
			D->ctrl[ActId] = 255.0;
			for (int32 i = 0; i < 500; ++i)
			{
				mj_step(M, D);
			}

			const double Q1L = (JntL >= 0) ? D->qpos[M->jnt_qposadr[JntL]] : 0.0;
			const double Q1R = (JntR >= 0) ? D->qpos[M->jnt_qposadr[JntR]] : 0.0;
			const double DeltaL = FMath::Abs(Q1L - Q0L);
			const double DeltaR = FMath::Abs(Q1R - Q0R);

			AddInfo(FString::Printf(
				TEXT("夹爪闭合 500 帧后：左指位移 %.4f rad，右指位移 %.4f rad，接触数 %d"),
				DeltaL, DeltaR, (int32)D->ncon));

			TestTrue(TEXT("夹爪执行器应能驱动指关节运动"),
				(DeltaL + DeltaR) > 1e-4);
			TestTrue(TEXT("闭合过程中应产生接触"), (int32)D->ncon > 0);
		}
		else
		{
			AddWarning(TEXT("未找到 gripper_fingers_actuator，跳过抓取物理验证"));
		}
	}

	S.Cleanup();
	return true;
}
