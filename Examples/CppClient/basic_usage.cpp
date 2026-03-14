// 参考:https://zhuanlan.zhihu.com/p/663460928
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

// 注意：hutb\Unreal\CarlaUE4\Plugins\Carla\CarlaDependencies\include\carla 不存在 carla/client 目录
#include <carla/client/ActorBlueprint.h>
#include <carla/client/BlueprintLibrary.h>
#include <carla/client/Client.h>
#include <carla/client/Sensor.h>
#include <carla/client/TimeoutException.h>
#include <carla/client/World.h>
#include <carla/geom/Transform.h>
#include <carla/actors/BlueprintLibrary.h>

using namespace std;
using namespace carla;

static auto ParseArguments(int argc, const char* argv[])
{
    //    EXPECT_TRUE((argc == 1u) || (argc == 3u));
    using ResultType = std::tuple<std::string, uint16_t>;
    return argc == 3u ? ResultType{ argv[1u], std::stoi(argv[2u]) } : ResultType{ "localhost", 2000u };
}

int main(int argc, const char* argv[])
{
    try
    {
        std::string host;
        uint16_t port;
        std::tie(host, port) = ParseArguments(argc, argv);

        client::Client client = client::Client(host, port);
        client.SetTimeout(40s);

        std::cout << "Client API version : " << client.GetClientVersion() << "\n";
        std::cout << "Server API version : " << client.GetServerVersion() << '\n';
        // 导入地图
        std::string town_name = "HutbCarlaCity";
        client::World world = client.LoadWorld(town_name);

        // 创建Waypoint。在Town05中，每隔2米获取一个Waypoint
        SharedPtr<client::Map> map = world.GetMap();
        std::vector<SharedPtr<client::Waypoint>> way_point = map->GenerateWaypoints(2);

        // 获取推荐的车辆其实位置
        std::vector<geom::Transform> spawn_point = map->GetRecommendedSpawnPoints();

        // 构建汽车对象
        // 获取这张地图中所有实物的蓝图
        SharedPtr<actors::BlueprintLibrary> blueprint_library = world.GetBlueprintLibrary();
        // 获取汽车的蓝图并指定其颜色
        actors::ActorBlueprint vehile_bp = *blueprint_library->Find("vehicle.tesla.model3");
        vehile_bp.SetAttribute("color", "255,255,255");
        std::vector<geom::Transform> recommend_points = map->GetRecommendedSpawnPoints();
        // 在推荐点中选取一点生成汽车
        SharedPtr<client::Actor> actor = world.SpawnActor(vehile_bp, recommend_points[300]);
        SharedPtr<client::Vehicle> vehicle = boost::static_pointer_cast<client::Vehicle>(actor);

        // 切换视角查看生成的汽车
        geom::Transform view_transform = recommend_points[300];
        SharedPtr<client::Actor> spectator = world.GetSpectator();
        view_transform.location -= 5.0f * view_transform.GetForwardVector();
        view_transform.location.z += 3.0f;
        view_transform.rotation.yaw += 0.0f;
        view_transform.rotation.pitch = -15.0f;
        spectator->SetTransform(view_transform);


        // 参考：https://zhuanlan.zhihu.com/p/663726656
        // 踩油门
        // client::Vehicle::Control control;
        // control.throttle = 0.3;
        // while (true)
        // {
        //     vehicle->ApplyControl(control);
        // }
        // 除了油门，还可以控制刹车、方向盘、传动比、手刹等
        // control.brake = 0.1;
        // control.steer = 0.02;
        // control.gear = 2;
        // control.hand_brake = true;

        // 速度加速度：AckermanControl

        // 自动驾驶
        vehicle->SetAutopilot(true);


    }
    catch (const client::TimeoutException& e)
    {
        std::cout << '\n' << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cout << "\nException: " << e.what() << std::endl;
        return 2;
    }

}