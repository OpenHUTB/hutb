// Copyright (c) 2025 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

//#include "carla/client/Client.h"


#include "carla/Sockets.h"
#include "carla/client/detail/Simulator.h"

#include "carla/trafficmanager/Constants.h"
#include "carla/trafficmanager/TrafficManager.h"
#include "carla/trafficmanager/TrafficManagerBase.h"
#include "carla/trafficmanager/TrafficManagerLocal.h"
#include "carla/trafficmanager/TrafficManagerRemote.h"

#define DEBUG_PRINT_TM  0
#define IP_DATA_BUFFER_SIZE     80

namespace carla {
namespace traffic_manager {

using namespace constants::SpeedThreshold;
using namespace constants::PID;

std::map<uint16_t, TrafficManagerBase*> TrafficManager::_tm_map;
std::mutex TrafficManager::_mutex;

/*
: _port(port)是构造函数的成员初始化列表
为什么不直接在函数体里写 _port = port;? 对 uint16_t 这种基本类型
两者效果几乎一样,但对类类型成员差别很大:而且有三类成员只能用初始化列表,
体内赋值直接编译报错:const 成员、引用成员、没有默认构造函数的类成员。

为什么这里也能调用到CreateTrafficManagerClient函数?  虽然下面有实现但是上面没有声明
由于头文件中包含了CreateTrafficManagerClient声明,然后这里又包含了头文件所以可以直接调用函数。
*/
TrafficManager::TrafficManager(
    carla::client::detail::EpisodeProxy episode_proxy,
    uint16_t port)
  : _port(port) {

  if(!GetTM(_port)){
    // Check if a TM server already exists and connect to it
    if(!CreateTrafficManagerClient(episode_proxy, port)) {
      // As TM server not running, create one
      CreateTrafficManagerServer(episode_proxy, port);
    }
  }
}

void TrafficManager::Release() {
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto& tm : _tm_map) {
    tm.second->Release();
    TrafficManagerBase *base_ptr = tm.second;
    delete base_ptr;
  }
  _tm_map.clear();
}

void TrafficManager::Reset() {
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto& tm : _tm_map) {
    tm.second->Reset();
  }
}

void TrafficManager::Tick() {
  std::lock_guard<std::mutex> lock(_mutex);
  for(auto& tm : _tm_map) {
    tm.second->SynchronousTick();
  }
}

void TrafficManager::ShutDown() {
  TrafficManagerBase* tm_ptr = GetTM(_port);
  std::lock_guard<std::mutex> lock(_mutex);
  auto it = _tm_map.find(_port);
  if (it != _tm_map.end()) {
    _tm_map.erase(it);
  }
  if(tm_ptr != nullptr) {
    tm_ptr->ShutDown();
    delete tm_ptr;
  }
}

void TrafficManager::CreateTrafficManagerServer(
    carla::client::detail::EpisodeProxy episode_proxy,
    uint16_t port) {

  // Get local IP details.
  /*
    auto GetLocalIP = [=](const uint16_t sport) -> std::pair<std::string, uint16_t> { ... };
    │                    │   │                      │
    │                    │   │                      └─ ④ 尾置返回类型:返回 (IP字符串, 端口) 二元组(显式返回类型）
    │                    │   └─ ③ 参数列表:接收一个端口号 sport
    │                    └─ ② 捕获列表:[=] = 按值使用外层变量，& 是按引用;[] 是不捕获
    └─ ① auto:定义局部变量,类型由右边推导 —— 右边是个 lambda,所以 GetLocalIP 是一个"可调用对象"
    GetLocalIP：一个"探测本机 IP 地址"的小工具函数
    []的应用：
    int main() {
      int a = 10;
      int b = 20;

      auto by_val = [=]() { return a + b; };   // 按值:创建时把 a、b 各拷一份进来
      auto by_ref = [&]() { return a + b; };   // 按引用:直接引用外面的 a、b
      auto only_a = [a]() { return a; };       // 点名:只捕获 a(按值)

      a = 100;   // 创建完 lambda 之后,改变外层变量

      std::cout << "值捕获 [=]  : " << by_val() << "   (创建时拷贝,冻结在 10+20)\n";
      std::cout << "引用捕获 [&] : " << by_ref() << "  (共享变量,看到新值 100+20)\n";
      std::cout << "点名 [a]    : " << only_a() << "   (只拷了 a,冻结在 10)\n";
    }
  */
  auto GetLocalIP = [=](const uint16_t sport)-> std::pair<std::string, uint16_t> {
    std::pair<std::string, uint16_t> localIP;
    /*
    socket( AF_INET,   SOCK_DGRAM, 0 )
            │          │           └─ 协议:0 = 自动选默认(对 UDP 而言就是 UDP)
            │          └─ 类型:数据报 → UDP(对比 SOCK_STREAM = TCP)
            └─ 协议族:IPv4
    */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == SOCK_INVALID_INDEX) {
      #if DEBUG_PRINT_TM
      std::cout << "Error number 1: " << errno << std::endl;
      std::cout << "Error message: " << strerror(errno) << std::endl;
      #endif
    } else {
      int err;
      sockaddr_in loopback;
      std::memset(&loopback, 0, sizeof(loopback));
      loopback.sin_family = AF_INET;
      loopback.sin_addr.s_addr = INADDR_LOOPBACK;
      loopback.sin_port = htons(9);
      err = connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback));
      if(err == SOCK_INVALID_INDEX) {
        #if DEBUG_PRINT_TM
        std::cout << "Error number 2: " << errno << std::endl;
        std::cout << "Error message: " << strerror(errno) << std::endl;
        #endif
      } else {
        socklen_t addrlen = sizeof(loopback);
        err = getsockname(sock, reinterpret_cast<struct sockaddr*> (&loopback), &addrlen);
        if(err == SOCK_INVALID_INDEX) {
          #if DEBUG_PRINT_TM
          std::cout << "Error number 3: " << errno << std::endl;
          std::cout << "Error message: " << strerror(errno) << std::endl;
          #endif
        } else {
          char buffer[IP_DATA_BUFFER_SIZE];
          const char* p = inet_ntop(AF_INET, &loopback.sin_addr, buffer, IP_DATA_BUFFER_SIZE);
          if(p != NULL) {
            localIP = std::pair<std::string, uint16_t>(std::string(buffer), sport);
          } else {
            #if DEBUG_PRINT_TM
            std::cout << "Error number 4: " << errno << std::endl;
            std::cout << "Error message: " << strerror(errno) << std::endl;
            #endif
          }
        }
      }
      #ifdef _WIN32
        closesocket(sock);
      #else
        close(sock);
      #endif
    }
    return localIP;
  };

  /// Define local constants
  const std::vector<float> longitudinal_param = LONGITUDIAL_PARAM;
  const std::vector<float> longitudinal_highway_param = LONGITUDIAL_HIGHWAY_PARAM;
  const std::vector<float> lateral_param = LATERAL_PARAM;
  const std::vector<float> lateral_highway_param = LATERAL_HIGHWAY_PARAM;
  const float perc_difference_from_limit = INITIAL_PERCENTAGE_SPEED_DIFFERENCE;

  std::pair<std::string, uint16_t> serverTM;

  /// 创建本地TM实例
  TrafficManagerLocal* tm_ptr = new TrafficManagerLocal(
    longitudinal_param,
    longitudinal_highway_param,
    lateral_param,
    lateral_highway_param,
    perc_difference_from_limit,
    episode_proxy,
    port);

  /// Get TM server info (Local IP & PORT)
  serverTM = GetLocalIP(port);

  /// Set this client as the TM to server
  episode_proxy.Lock()->AddTrafficManagerRunning(serverTM);

  #if DEBUG_PRINT_TM
  /// Print status
  std::cout << "NEW@: Registered TM at "
        << serverTM.first  << ":"
        << serverTM.second << " ..... SUCCESS."
        << std::endl;
  #endif

  /// Set the pointer of the instance
  _tm_map.insert(std::make_pair(port, tm_ptr));

}

bool TrafficManager::CreateTrafficManagerClient(
    carla::client::detail::EpisodeProxy episode_proxy,
    uint16_t port) {

  bool result = false;

  if(episode_proxy.Lock()->IsTrafficManagerRunning(port)) {

    /// Get TM server info (Remote IP & PORT)
    std::pair<std::string, uint16_t> serverTM =
      episode_proxy.Lock()->GetTrafficManagerRunning(port);

    /// Set remote TM server IP and port
    TrafficManagerRemote* tm_ptr = new(std::nothrow)
      TrafficManagerRemote(serverTM, episode_proxy);

    /// Try to connect to remote TM server
    try {

      /// Check memory allocated or not
      if(tm_ptr != nullptr) {

        #if DEBUG_PRINT_TM
        // Test print
        std::cout << "OLD@: Registered TM at "
              << serverTM.first  << ":"
              << serverTM.second << " ..... TRY "
              << std::endl;
        #endif
        /// Try to reset all traffic lights
        tm_ptr->HealthCheckRemoteTM();

        /// Set the pointer of the instance
        _tm_map.insert(std::make_pair(port, tm_ptr));

        result = true;
      }
    }

    /// If Connection error occurred
    catch (...) {

      /// Clear previously allocated memory
      delete tm_ptr;

      #if DEBUG_PRINT_TM
      /// Test print
      std::cout 	<< "OLD@: Registered TM at "
            << serverTM.first  << ":"
            << serverTM.second << " ..... FAILED "
            << std::endl;
      #endif
    }

  }

  return result;
}

} // namespace traffic_manager
} // namespace carla
