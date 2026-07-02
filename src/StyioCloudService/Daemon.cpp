#include "StyioCloudService/Config.hpp"
#include "StyioCloudService/BeastServer.hpp"
#include "StyioCloudService/Http.hpp"
#include "StyioCloudService/Identity.hpp"
#include "StyioCloudService/PostgresStore.hpp"
#include "StyioCloudService/Router.hpp"

#include <iostream>
#include <string>

namespace
{

void PrintUsage()
{
  std::cout
      << "Usage: styio-cloudd [--check-config|--self-test|--print-routes|--print-migrations]\n";
}

styio::cloud::MtlsIdentity SelfTestIdentity()
{
  return {
      .role = "operator",
      .tenant_id = "tenant-demo",
      .node_id = "node-local",
  };
}

}  // namespace

int main(int argc, char **argv)
{
  const std::string command = argc > 1 ? argv[1] : "--check-config";
  styio::cloud::StyioCloudConfig config = styio::cloud::LoadStyioCloudConfigFromEnvironment();
  styio::cloud::StyioCloudRouter router(config);

  if (command == "--help" || command == "-h")
  {
    PrintUsage();
    return 0;
  }
  if (command == "--check-config")
  {
    nlohmann::json payload = styio::cloud::SerializePublicConfig(config);
    payload["http_adapter"] = styio::cloud::DescribeBeastServerCapability();
    std::cout << payload.dump(2) << "\n";
    return 0;
  }
  if (command == "--print-routes")
  {
    nlohmann::json routes = nlohmann::json::array();
    for (const styio::cloud::RouteSpec &route : router.routes())
    {
      routes.push_back({
          {"operation_id", route.operation_id},
          {"method", styio::cloud::ToString(route.method)},
          {"path", route.path},
          {"internal", route.internal},
      });
    }
    std::cout << routes.dump(2) << "\n";
    return 0;
  }
  if (command == "--print-migrations")
  {
    for (const styio::cloud::SqlMigration &migration : styio::cloud::CloudKernelMigrations())
    {
      std::cout << "-- " << migration.id << "\n" << migration.sql << "\n";
    }
    return 0;
  }
  if (command == "--self-test")
  {
    const styio::cloud::HttpResponse health = router.Dispatch({
        .method = styio::cloud::HttpMethod::Get,
        .path = "/health",
        .identity = SelfTestIdentity(),
    });
    const styio::cloud::HttpResponse submit = router.Dispatch({
        .method = styio::cloud::HttpMethod::Post,
        .path = "/jobs",
        .body = {
            {"tenant_id", "tenant-demo"},
            {"workspace_id", "workspace-demo"},
            {"action", "build"},
            {"preferred_worker_pool", "linux/x86_64/build/nightly/minimal"},
            {"job_request", {{"schema_version", 1}, {"action", "build"}}},
        },
        .identity = SelfTestIdentity(),
    });
    std::cout << nlohmann::json{{"health", health.body}, {"submit", submit.body}}.dump(2) << "\n";
    return health.status_code == 200 && submit.status_code == 200 ? 0 : 1;
  }

  std::cerr << "unsupported command: " << command << "\n";
  PrintUsage();
  return 2;
}
