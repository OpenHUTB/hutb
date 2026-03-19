// 参考:https://zhuanlan.zhihu.com/p/663460928
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

// 注意：hutb\Unreal\CarlaUE4\Plugins\Carla\CarlaDependencies\include\carla 不存在 carla/client 目录
#include <carla/actors/ActorBlueprint.h>
#include <carla/actors/BlueprintLibrary.h>
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


// 随机选取
template <typename RangeT, typename RNG>
static auto& RandomChoice(const RangeT& range, RNG&& generator)
{
    std::uniform_int_distribution<size_t> dist{ 0u, range.size() - 1u };
    return range[dist(std::forward<RNG>(generator))];
}

// 一个创建汽车的函数，方便循环使用
SharedPtr<client::Vehicle> SetVehicle(geom::Transform transform, client::World world)
{
    std::mt19937_64 rng((std::random_device())());

    // Get a random vehicle blueprint.
    SharedPtr<actors::BlueprintLibrary> blueprint_library = world.GetBlueprintLibrary();
    auto vehicles = blueprint_library->Filter("vehicle");
    // 在容器中随机选取车型
    actors::BlueprintLibrary vehicle_bp = RandomChoice(*vehicles, rng);

    SharedPtr<client::Actor> actor = world.SpawnActor(vehicle_bp, transform);
    SharedPtr<client::Vehicle> vehicle = boost::static_pointer_cast<client::Vehicle>(actor);
    // 将汽车全部设置为自动驾驶模式
    vehicle->SetAutopilot(true);
    return vehicle;
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


        // ********************************************************************************************
        // 构建汽车对象********************************************************************************
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
        // vehicle->SetAutopilot(true);
        // std:cout << "Autopilot enabled" << std::endl;
        // ********************************************************************************************



        // ********************************************************************************************
        // 添加行人************************************************************************************
        actors::ActorBlueprint walker_bp = *blueprint_library->Find("walker.pedestrian.0037");
        SharedPtr<client::Actor> walker_actor = world.SpawnActor(walker_bp, recommend_points[200]);
        SharedPtr<client::Walker> walker = boost::static_pointer_cast<client::Walker>(walker_actor);
        // 切换视角查看生成的行人
        view_transform = recommend_points[200];
        spectator = world.GetSpectator();
        view_transform.location -= 5.0f * view_transform.GetForwardVector();
        view_transform.location.z += 3.0f;
        view_transform.rotation.yaw += 0.0f;
        view_transform.rotation.pitch = -15.0f;
        spectator->SetTransform(view_transform);
        // 让行人动起来
        client::Walker::Control walker_control;
        walker_control.speed = 0.2;
        walker_control.jump = false; // 爬坡的时候使用
        walker_control.direction = { recommend_points[201].location.x,recommend_points[200].location.y,0.0 };
        walker->ApplyControl(walker_control);
        // ********************************************************************************************

        


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