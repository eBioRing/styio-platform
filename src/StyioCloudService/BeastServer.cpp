#include "StyioCloudService/BeastServer.hpp"

#if __has_include(<boost/asio.hpp>) && __has_include(<boost/beast.hpp>)
#define STYIO_CLOUD_HAS_BOOST_BEAST 1
#else
#define STYIO_CLOUD_HAS_BOOST_BEAST 0
#endif

namespace styio::cloud
{

nlohmann::json DescribeBeastServerCapability()
{
  return {
      {"target", "Boost.Beast/Asio"},
      {"headers_available", STYIO_CLOUD_HAS_BOOST_BEAST == 1},
      {"role", "production HTTP adapter for StyioCloudRouter"},
      {"fallback", "StyioCloudRouter remains directly testable without network dependencies"},
  };
}

}  // namespace styio::cloud
