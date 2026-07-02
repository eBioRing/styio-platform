#pragma once

#include "StyioCloudService/Config.hpp"
#include "StyioCloudService/Http.hpp"
#include "StyioCloudService/JobQueue.hpp"

#include <map>
#include <string>
#include <vector>

namespace styio::cloud
{

struct RegistryMirrorState
{
  std::string mirror_id;
  std::string origin;
  std::string freshness = "lagging";
  std::string replay_cursor = "checkpoint-0000";
};

class StyioCloudRouter
{
public:
  explicit StyioCloudRouter(StyioCloudConfig config);

  const StyioCloudConfig &config() const { return config_; }
  const std::vector<RouteSpec> &routes() const { return routes_; }
  HttpResponse Dispatch(const HttpRequest &request);

private:
  HttpResponse RequireIdentity(const RouteMatch &match, const HttpRequest &request) const;
  HttpResponse HandleHealth() const;
  HttpResponse HandleNodeSelf() const;
  HttpResponse HandleSubmitJob(const HttpRequest &request);
  HttpResponse HandleGetJob(const RouteMatch &match) const;
  HttpResponse HandleGetJobEvents(const RouteMatch &match) const;
  HttpResponse HandleCancelJob(const RouteMatch &match, const HttpRequest &request);
  HttpResponse HandleRegisterWorker(const HttpRequest &request);
  HttpResponse HandleClaimJob(const HttpRequest &request);
  HttpResponse HandleHeartbeatJob(const RouteMatch &match, const HttpRequest &request);
  HttpResponse HandleCompleteJob(const RouteMatch &match, const HttpRequest &request);
  HttpResponse HandleMirrorStatus(const RouteMatch &match) const;
  HttpResponse HandleRegistryStatus() const;
  HttpResponse HandlePublishRelease(const HttpRequest &request);
  HttpResponse HandleVerifyRegistry(const HttpRequest &request);
  void RecordMirrorState(std::string freshness, std::string replay_cursor);

  StyioCloudConfig config_;
  std::vector<RouteSpec> routes_;
  std::map<std::string, StyioCloudJobRecord> jobs_;
  std::map<std::string, std::vector<JobEventRecord>> events_;
  std::map<std::string, nlohmann::json> workers_;
  std::map<std::string, RegistryMirrorState> mirrors_;
  std::map<std::string, nlohmann::json> published_releases_;
};

std::vector<RouteSpec> BuildStyioCloudControlPlaneRoutes();
std::vector<RouteSpec> BuildRegistryControlPlaneRoutes();

}  // namespace styio::cloud
