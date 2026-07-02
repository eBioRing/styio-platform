#include "BuildTestSupport.hpp"

#include "PafioCloud/Contract.hpp"
#include "PafioCloud/Execution.hpp"
#include "PafioCloud/Job.hpp"
#include "PafioCore/Errors.hpp"
#include "PafioPlan/CompilePlan.hpp"
#include "StyioCloudService/Http.hpp"
#include "StyioCloudService/Identity.hpp"
#include "StyioCloudService/ObjectStore.hpp"
#include "StyioCloudService/PostgresStore.hpp"
#include "StyioCloudService/Router.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

using json = nlohmann::json;

using pafio::testsupport::CanonicalAbsolutePath;
using pafio::testsupport::MakeTempDir;
using pafio::testsupport::ReadFile;
using pafio::testsupport::WriteFile;

TEST(StyioCloudCompilePlanTests, WritesCompilePlanForSingleLibPackage)
{
  const fs::path root = MakeTempDir("platform-single-lib-plan");
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/demo\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");
  WriteFile(root / "src/lib.styio", "# value := 1\n");

  const pafio::BuildPlanResult result = pafio::WriteBuildCompilePlan({
      .manifest_path = root / "pafio.toml",
      .select_lib = true,
  });

  EXPECT_TRUE(fs::exists(result.plan_path));
  EXPECT_EQ(result.entry_target_kind, "lib");
  EXPECT_EQ(result.entry_package_name, "acme/demo");

  const json plan = json::parse(ReadFile(result.plan_path));
  EXPECT_EQ(plan["plan_version"], 1);
  EXPECT_EQ(plan["intent"], "build");
  EXPECT_EQ(plan["workspace_root"], CanonicalAbsolutePath(root).string());
  EXPECT_EQ(plan["entry"]["target_kind"], "lib");
  EXPECT_EQ(plan["entry"]["file"], CanonicalAbsolutePath(root / "src/lib.styio").string());
  EXPECT_EQ(plan["toolchain"]["std_package_id"], "builtin:std@nightly/2026");
  EXPECT_EQ(plan["profile"]["name"], "dev");
  EXPECT_EQ(plan["emit"]["error_format"], "jsonl");
  ASSERT_EQ(plan["packages"].size(), 1U);
  EXPECT_EQ(plan["packages"][0]["targets"]["lib"], CanonicalAbsolutePath(root / "src/lib.styio").string());
}

TEST(StyioCloudCompilePlanTests, RejectsMixedEditionGraphForCompilePlanV1)
{
  const fs::path root = MakeTempDir("platform-mixed-edition-plan");
  WriteFile(
      root / "pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"deps/util\" }\n");
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");
  WriteFile(
      root / "deps/util/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/util\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2027\"\n"
      "publish = false\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");
  WriteFile(root / "deps/util/src/lib.styio", "# util := 1\n");

  EXPECT_THROW(
      pafio::WriteBuildCompilePlan({
          .manifest_path = root / "pafio.toml",
      }),
      pafio::PlanError);
}

TEST(StyioCloudJobTests, EmitsControlPlaneBuildJobRequest)
{
  const pafio::ProjectToolchainState state = {
      .manifest_path = "pafio.toml",
      .state_path = "pafio-toolchain.lock",
      .state_file_exists = true,
      .mode = "build",
      .channel = "nightly",
      .build_mode = "minimal",
      .risk_class = "trusted-internal",
      .preferred_execution_lane = "warm-shared",
      .security_profile = "trusted-warm",
      .source_revision = std::string("nightly-head"),
  };
  const pafio::BuildPlanRequest request = {
      .manifest_path = "pafio.toml",
      .package_name = std::string("acme/demo"),
      .bin_name = std::string("demo"),
      .profile = "dev",
      .build_mode = "minimal",
  };
  const pafio::WorkflowInvocationOptions options = {
      .non_interactive = true,
      .source_revision = std::string("nightly-head"),
  };

  const pafio::CloudExecutionPolicy policy = pafio::ResolveCloudExecutionPolicy(state);
  const pafio::CloudBuildJobRequest job = pafio::BuildCloudBuildJobRequest("build", request, state, options, policy);
  const json payload = pafio::BuildCloudBuildJobRequestPayload(job);

  EXPECT_EQ(payload.at("api_path").get<std::string>(), "/api/styio-cloud/v1/jobs");
  EXPECT_EQ(payload.at("action").get<std::string>(), "build");
  EXPECT_EQ(payload.at("toolchain").at("mode").get<std::string>(), "build");
  EXPECT_EQ(payload.at("toolchain").at("build_mode").get<std::string>(), "minimal");
  EXPECT_EQ(payload.at("cloud").at("execution_lane").get<std::string>(), "warm-shared");
  EXPECT_EQ(payload.at("cloud").at("worker_pool_key").at("toolchain_mode").get<std::string>(), "build");
  EXPECT_TRUE(payload.at("cloud").at("cache_policy").at("worker_local_reuse").get<bool>());
  EXPECT_EQ(payload.at("source").at("requested_revision").get<std::string>(), "nightly-head");
  EXPECT_TRUE(payload.at("source").at("non_interactive").get<bool>());
}

TEST(StyioCloudJobTests, RejectsSourceBuildOverridesWhenProjectUsesBinaryMode)
{
  const pafio::ProjectToolchainState state = {
      .manifest_path = "pafio.toml",
      .state_path = "pafio-toolchain.lock",
      .state_file_exists = true,
      .mode = "binary",
      .channel = "stable",
      .build_mode = "minimal",
  };
  const pafio::BuildPlanRequest request = {
      .manifest_path = "pafio.toml",
      .intent = "build",
      .profile = "dev",
  };
  const pafio::WorkflowInvocationOptions options = {
      .source_revision = std::string("nightly-head"),
  };

  try
  {
    (void) pafio::BuildCloudBuildJobRequest("build", request, state, options, pafio::ResolveCloudExecutionPolicy(state));
    FAIL() << "expected ValidationError";
  }
  catch (const pafio::ValidationError &error)
  {
    EXPECT_EQ(std::string(error.what()), "source-build options require 'pafio use build'");
  }
}

namespace
{

styio::cloud::MtlsIdentity WorkerIdentity()
{
  return {
      .role = "worker",
      .tenant_id = "tenant-acme",
      .node_id = "worker-01",
  };
}

styio::cloud::MtlsIdentity RegistryWriterIdentity()
{
  return {
      .role = "registry-writer",
      .tenant_id = "tenant-acme",
      .node_id = "registry-writer-01",
  };
}

styio::cloud::MtlsIdentity MirrorIdentity()
{
  return {
      .role = "mirror",
      .tenant_id = "tenant-acme",
      .node_id = "mirror-01",
  };
}

nlohmann::json MinimalJobRequest()
{
  return {
      {"tenant_id", "tenant-acme"},
      {"workspace_id", "workspace-main"},
      {"action", "build"},
      {"region", "local-dev"},
      {"preferred_worker_pool", "linux/x86_64/build/nightly/minimal"},
      {"job_request", {
                          {"manifest_path", "pafio.toml"},
                          {"profile", "dev"},
                      }},
  };
}

styio::cloud::HttpRequest Request(
    styio::cloud::HttpMethod method,
    std::string path,
    nlohmann::json body = nlohmann::json::object())
{
  return {
      .method = method,
      .path = std::move(path),
      .body = std::move(body),
      .identity = WorkerIdentity(),
  };
}

styio::cloud::HttpRequest RequestWithIdentity(
    styio::cloud::HttpMethod method,
    std::string path,
    styio::cloud::MtlsIdentity identity,
    nlohmann::json body = nlohmann::json::object())
{
  return {
      .method = method,
      .path = std::move(path),
      .body = std::move(body),
      .identity = std::move(identity),
  };
}

styio::cloud::StyioCloudConfig TestStyioCloudConfig(const fs::path &root)
{
  styio::cloud::StyioCloudConfig config;
  config.region = "local-dev";
  config.node_id = "node-test";
  config.postgres_dsn = "postgres://platform@localhost/styio";
  config.object_store.provider = "memory";
  config.registry.root = (root / "registry").string();
  config.registry.key_dir = (root / "keys").string();
  config.registry.registry_name = "test-registry";
  config.registry.mirror_id = "mirror-local";
  config.registry.mirror_origin = "registry-primary";
  config.mtls.required = true;
  return config;
}

}  // namespace

TEST(StyioCloudServiceIdentityTests, ParsesMtlsUriSanIntoRoleTenantAndNode)
{
  const std::optional<styio::cloud::MtlsIdentity> identity =
      styio::cloud::ParseMtlsUriSan("spiffe://styio-cloud/tenant/tenant-acme/role/worker/node/worker-01");

  ASSERT_TRUE(identity.has_value());
  EXPECT_EQ(identity->role, "worker");
  EXPECT_EQ(identity->tenant_id, "tenant-acme");
  EXPECT_EQ(identity->node_id, "worker-01");
  EXPECT_TRUE(styio::cloud::IsStyioCloudServiceRole(identity->role));

  const json serialized = styio::cloud::SerializeMtlsIdentity(*identity);
  EXPECT_EQ(serialized.at("role").get<std::string>(), "worker");
  EXPECT_EQ(serialized.at("tenant_id").get<std::string>(), "tenant-acme");
  EXPECT_EQ(serialized.at("node_id").get<std::string>(), "worker-01");
}

TEST(StyioCloudServiceIdentityTests, RejectsUnknownMtlsUriSanRoleOrMissingNode)
{
  EXPECT_FALSE(styio::cloud::ParseMtlsUriSan("spiffe://styio-cloud/tenant/acme/role/browser/node/client").has_value());
  EXPECT_FALSE(styio::cloud::ParseMtlsUriSan("spiffe://styio-cloud/tenant/acme/role/worker").has_value());
  EXPECT_FALSE(styio::cloud::ParseMtlsUriSan("https://styio-cloud/tenant/acme/role/worker/node/worker-01").has_value());
}

TEST(StyioCloudServiceRouterTests, MatchesRouteParametersForJobsAndMirrors)
{
  const std::vector<styio::cloud::RouteSpec> routes = styio::cloud::BuildStyioCloudControlPlaneRoutes();

  const std::optional<styio::cloud::RouteMatch> job =
      styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Get, "/jobs/job-abc/events");
  ASSERT_TRUE(job.has_value());
  EXPECT_EQ(job->route.operation_id, "getJobEvents");
  EXPECT_EQ(job->parameters.at("job_id"), "job-abc");

  const std::optional<styio::cloud::RouteMatch> mirror =
      styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Get, "/mirrors/registry-primary/status");
  ASSERT_TRUE(mirror.has_value());
  EXPECT_EQ(mirror->route.operation_id, "mirrorStatus");
  EXPECT_EQ(mirror->parameters.at("mirror_id"), "registry-primary");

  EXPECT_FALSE(styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Post, "/jobs/job-abc/events").has_value());
}

TEST(StyioCloudServiceRouterTests, MatchesRegistryControlPlaneRoutesWithContractBasePath)
{
  const std::vector<styio::cloud::RouteSpec> routes = styio::cloud::BuildRegistryControlPlaneRoutes();

  const std::optional<styio::cloud::RouteMatch> status =
      styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Get, "/api/pafio-registry-control/v1/status");
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(status->route.operation_id, "registryStatus");
  EXPECT_TRUE(status->route.internal);

  const std::optional<styio::cloud::RouteMatch> publish =
      styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Post, "/api/pafio-registry-control/v1/publish");
  ASSERT_TRUE(publish.has_value());
  EXPECT_EQ(publish->route.operation_id, "publishRelease");

  const std::optional<styio::cloud::RouteMatch> verify =
      styio::cloud::MatchRoute(routes, styio::cloud::HttpMethod::Post, "/api/pafio-registry-control/v1/verify");
  ASSERT_TRUE(verify.has_value());
  EXPECT_EQ(verify->route.operation_id, "verifyRegistry");
}

TEST(StyioCloudServiceObjectStoreTests, SanitizesArtifactObjectKeyParts)
{
  const std::string key = styio::cloud::BuildArtifactObjectKey(
      "tenant/acme",
      "workspace main",
      "job:42",
      "../out.tar.gz");

  EXPECT_EQ(key, "tenants/tenant_acme/workspaces/workspace_main/jobs/job_42/artifacts/.._out.tar.gz");
  EXPECT_EQ(key.find("tenant/acme"), std::string::npos);
  EXPECT_EQ(key.find("workspace main"), std::string::npos);
  EXPECT_EQ(key.find("job:42"), std::string::npos);
  EXPECT_EQ(key.find("../out"), std::string::npos);
}

TEST(StyioCloudServicePostgresTests, DefinesCloudKernelMigrationAndClaimSql)
{
  const std::vector<styio::cloud::SqlMigration> migrations = styio::cloud::CloudKernelMigrations();

  ASSERT_FALSE(migrations.empty());
  EXPECT_EQ(migrations.front().id, "001_cloud_kernel");
  EXPECT_NE(migrations.front().sql.find("CREATE TABLE IF NOT EXISTS platform_jobs"), std::string::npos);
  EXPECT_NE(migrations.front().sql.find("CREATE TABLE IF NOT EXISTS platform_job_events"), std::string::npos);
  EXPECT_NE(migrations.front().sql.find("CREATE TABLE IF NOT EXISTS platform_artifacts"), std::string::npos);

  const std::string claim_sql = styio::cloud::ClaimJobSql();
  EXPECT_NE(claim_sql.find("FOR UPDATE SKIP LOCKED"), std::string::npos);
  EXPECT_NE(claim_sql.find("worker_pool_key = $2"), std::string::npos);
  EXPECT_NE(claim_sql.find("RETURNING *"), std::string::npos);

  const std::string complete_sql = styio::cloud::CompleteJobSql();
  EXPECT_NE(complete_sql.find("status = 'running'"), std::string::npos);
  EXPECT_NE(complete_sql.find("worker_id = $4"), std::string::npos);
}

TEST(StyioCloudServiceJobQueueTests, SubmitClaimCompleteLifecycleUsesSuccessEnvelopes)
{
  styio::cloud::StyioCloudConfig config;
  config.region = "local-dev";
  config.node_id = "node-test";
  config.postgres_dsn = "postgres://platform@localhost/styio";
  config.object_store.provider = "memory";
  config.mtls.required = true;

  styio::cloud::StyioCloudRouter router(config);

  const styio::cloud::HttpResponse submit =
      router.Dispatch(Request(styio::cloud::HttpMethod::Post, "/jobs", MinimalJobRequest()));
  ASSERT_EQ(submit.status_code, 200);
  ASSERT_EQ(submit.body.at("returncode").get<int>(), 0);
  EXPECT_EQ(submit.body.at("message").get<std::string>(), "queued platform job");
  const json queued = submit.body.at("payload");
  const std::string job_id = queued.at("job_id").get<std::string>();
  EXPECT_EQ(queued.at("status").get<std::string>(), "queued");
  EXPECT_EQ(queued.at("worker_pool_key").get<std::string>(), "linux/x86_64/build/nightly/minimal");

  const styio::cloud::HttpResponse register_worker =
      router.Dispatch(Request(
          styio::cloud::HttpMethod::Post,
          "/workers/register",
          {
              {"worker_id", "worker-01"},
              {"region", "local-dev"},
              {"worker_pool_key", "linux/x86_64/build/nightly/minimal"},
              {"capacity", 1},
          }));
  ASSERT_EQ(register_worker.status_code, 200);
  EXPECT_EQ(register_worker.body.at("payload").at("status").get<std::string>(), "registered");

  const styio::cloud::HttpResponse claim =
      router.Dispatch(Request(
          styio::cloud::HttpMethod::Post,
          "/jobs/claim",
          {
              {"worker_id", "worker-01"},
              {"region", "local-dev"},
              {"worker_pool_key", "linux/x86_64/build/nightly/minimal"},
          }));
  ASSERT_EQ(claim.status_code, 200);
  ASSERT_TRUE(claim.body.at("payload").at("claimed").get<bool>());
  EXPECT_EQ(claim.body.at("payload").at("job").at("job_id").get<std::string>(), job_id);
  EXPECT_EQ(claim.body.at("payload").at("job").at("status").get<std::string>(), "running");
  EXPECT_EQ(claim.body.at("payload").at("job").at("worker_id").get<std::string>(), "worker-01");

  const std::string artifact_key = styio::cloud::BuildArtifactObjectKey("tenant-acme", "workspace-main", job_id, "stdout.log");
  const styio::cloud::HttpResponse complete =
      router.Dispatch(Request(
          styio::cloud::HttpMethod::Post,
          "/jobs/" + job_id + "/complete",
          {
              {"worker_id", "worker-01"},
              {"status", "succeeded"},
              {"message", "build completed"},
              {"artifacts", json::array({
                                {
                                    {"artifact_id", "stdout"},
                                    {"object_key", artifact_key},
                                    {"kind", "log"},
                                },
                            })},
          }));
  ASSERT_EQ(complete.status_code, 200);
  ASSERT_EQ(complete.body.at("returncode").get<int>(), 0);
  EXPECT_EQ(complete.body.at("message").get<std::string>(), "completed platform job");
  EXPECT_EQ(complete.body.at("payload").at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(complete.body.at("payload").at("artifacts").at(0).at("object_key").get<std::string>(), artifact_key);

  const styio::cloud::HttpResponse events =
      router.Dispatch(Request(styio::cloud::HttpMethod::Get, "/jobs/" + job_id + "/events"));
  ASSERT_EQ(events.status_code, 200);
  const json event_list = events.body.at("payload").at("events");
  ASSERT_EQ(event_list.size(), 3U);
  EXPECT_EQ(event_list.at(0).at("status").get<std::string>(), "queued");
  EXPECT_EQ(event_list.at(1).at("status").get<std::string>(), "running");
  EXPECT_EQ(event_list.at(2).at("status").get<std::string>(), "succeeded");
}

TEST(StyioCloudRegistryControlPlaneTests, StatusUsesRedactedPathsAndFilesystemReadiness)
{
  const fs::path root = MakeTempDir("platform-registry-status");
  const styio::cloud::StyioCloudConfig config = TestStyioCloudConfig(root);
  WriteFile(fs::path(config.registry.root) / "config.json", "{}\n");
  WriteFile(fs::path(config.registry.root) / "trust/root.json", "{}\n");
  fs::create_directories(config.registry.key_dir);

  styio::cloud::StyioCloudRouter router(config);
  const styio::cloud::HttpResponse status = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Get,
      "/api/pafio-registry-control/v1/status",
      RegistryWriterIdentity()));

  ASSERT_EQ(status.status_code, 200);
  ASSERT_EQ(status.body.at("returncode").get<int>(), 0);
  const json payload = status.body.at("payload");
  EXPECT_EQ(payload.at("registry_root").get<std::string>(), "<redacted>");
  EXPECT_EQ(payload.at("key_dir").get<std::string>(), "<redacted>");
  EXPECT_EQ(payload.at("registry_name").get<std::string>(), "test-registry");
  EXPECT_TRUE(payload.at("root_initialized").get<bool>());
  EXPECT_TRUE(payload.at("config_present").get<bool>());
  EXPECT_TRUE(payload.at("root_metadata_present").get<bool>());
  EXPECT_EQ(payload.at("publish_endpoint").get<std::string>(), "/api/pafio-registry-control/v1/publish");
  EXPECT_EQ(payload.at("verify_endpoint").get<std::string>(), "/api/pafio-registry-control/v1/verify");
  EXPECT_EQ(status.body.dump().find(config.registry.root), std::string::npos);
  EXPECT_EQ(status.body.dump().find(config.registry.key_dir), std::string::npos);
}

TEST(StyioCloudRegistryControlPlaneTests, PublishVerifyAndMirrorStatusUseLocalState)
{
  const fs::path root = MakeTempDir("platform-registry-publish");
  const styio::cloud::StyioCloudConfig config = TestStyioCloudConfig(root);
  WriteFile(
      root / "workspace/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"demo/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");

  styio::cloud::StyioCloudRouter router(config);
  const styio::cloud::HttpResponse publish = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/publish",
      RegistryWriterIdentity(),
      {
          {"manifest_path", (root / "workspace/pafio.toml").string()},
          {"publisher_id", "registry-writer-01"},
      }));

  ASSERT_EQ(publish.status_code, 200);
  ASSERT_EQ(publish.body.at("returncode").get<int>(), 0);
  const json published = publish.body.at("payload");
  EXPECT_EQ(published.at("package").get<std::string>(), "demo/app");
  EXPECT_EQ(published.at("version").get<std::string>(), "0.1.0");
  EXPECT_TRUE(published.at("created_root").get<bool>());
  EXPECT_EQ(published.at("sequence").get<int>(), 1);
  EXPECT_EQ(published.at("archive_sha256").get<std::string>().size(), 64U);
  EXPECT_TRUE(fs::exists(fs::path(config.registry.root) / published.at("artifact_path").get<std::string>()));
  EXPECT_TRUE(fs::exists(fs::path(config.registry.root) / published.at("index_path").get<std::string>()));
  EXPECT_TRUE(fs::exists(fs::path(config.registry.root) / published.at("log_leaf_path").get<std::string>()));

  const styio::cloud::HttpResponse verify = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/verify",
      MirrorIdentity(),
      json::object()));
  ASSERT_EQ(verify.status_code, 200);
  EXPECT_TRUE(verify.body.at("payload").at("ok").get<bool>());
  EXPECT_EQ(verify.body.at("payload").at("namespaces").get<int>(), 1);
  EXPECT_EQ(verify.body.at("payload").at("index_files").get<int>(), 1);
  EXPECT_EQ(verify.body.at("payload").at("releases").get<int>(), 1);
  EXPECT_EQ(verify.body.at("payload").at("tree_size").get<int>(), 1);

  const styio::cloud::HttpResponse mirror = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Get,
      "/mirrors/mirror-local/status",
      MirrorIdentity()));
  ASSERT_EQ(mirror.status_code, 200);
  EXPECT_EQ(mirror.body.at("payload").at("freshness").get<std::string>(), "fresh");
  EXPECT_EQ(mirror.body.at("payload").at("replay_cursor").get<std::string>(), "checkpoint-0001");
}

TEST(StyioCloudRegistryControlPlaneTests, EnforcesRegistryRolesAndNonSuccessDomainErrors)
{
  const fs::path root = MakeTempDir("platform-registry-errors");
  const styio::cloud::StyioCloudConfig config = TestStyioCloudConfig(root);
  WriteFile(
      root / "workspace/pafio.toml",
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"demo/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[toolchain]\n"
      "channel = \"nightly\"\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n");

  styio::cloud::StyioCloudRouter router(config);
  const json publish_request = {
      {"manifest_path", (root / "workspace/pafio.toml").string()},
      {"publisher_id", "registry-writer-01"},
  };

  const styio::cloud::HttpResponse denied = router.Dispatch(Request(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/publish",
      publish_request));
  ASSERT_EQ(denied.status_code, 403);
  EXPECT_EQ(denied.body.at("returncode").get<int>(), 2);

  const styio::cloud::HttpResponse verify_before_publish = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/verify",
      MirrorIdentity(),
      json::object()));
  ASSERT_EQ(verify_before_publish.status_code, 422);
  EXPECT_EQ(verify_before_publish.body.at("error_payload").at("category").get<std::string>(), "VerifyError");

  const styio::cloud::HttpResponse first_publish = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/publish",
      RegistryWriterIdentity(),
      publish_request));
  ASSERT_EQ(first_publish.status_code, 200);

  const styio::cloud::HttpResponse duplicate = router.Dispatch(RequestWithIdentity(
      styio::cloud::HttpMethod::Post,
      "/api/pafio-registry-control/v1/publish",
      RegistryWriterIdentity(),
      publish_request));
  ASSERT_EQ(duplicate.status_code, 409);
  EXPECT_EQ(duplicate.body.at("returncode").get<int>(), 17);
  EXPECT_EQ(duplicate.body.at("error_payload").at("category").get<std::string>(), "PublishError");
  EXPECT_FALSE(duplicate.body.at("error_payload").contains("operation_id"));
}
