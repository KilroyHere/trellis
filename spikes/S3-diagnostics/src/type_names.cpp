// Spike S3 (throwaway): constexpr type names from __PRETTY_FUNCTION__, checked at
// compile time where the spelling is the same on every compiler, printed for the rest.
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "mock.hpp"
#include "type_name.hpp"

struct IRepository {
  virtual ~IRepository() = default;
};
namespace app::detail {
struct Service {};
}  // namespace app::detail
namespace {
struct Hidden {};
}  // namespace
template <int N>
struct Fixed {};

// Compile-time proof that the names are constant expressions.
static_assert(s3::type_name<int>() == "int");
static_assert(s3::type_name<IRepository>() == "IRepository");
static_assert(s3::type_name<app::detail::Service>() == "app::detail::Service");
static_assert(s3::type_name<std::shared_ptr<IRepository>>() == "std::shared_ptr<IRepository>");
static_assert(s3::type_name<trellis::error::path<IRepository, app::detail::Service>>() ==
              "trellis::error::path<IRepository, app::detail::Service>");

template <class T>
void show(const char* label) {
  constexpr std::string_view name = s3::type_name<T>();
  std::printf("%-28s %.*s\n", label, static_cast<int>(name.size()), name.data());
}

int main() {
  auto lambda = [] {};
  show<const IRepository&>("const IRepository&");
  show<IRepository*>("IRepository*");
  show<std::unique_ptr<IRepository>>("std::unique_ptr<IRepository>");
  show<std::string>("std::string");
  show<std::vector<int>>("std::vector<int>");
  show<Hidden>("anonymous-namespace type");
  show<Fixed<3>>("Fixed<3>");
  show<decltype(lambda)>("lambda");
  show<trellis::singleton>("trellis::singleton (inline v1)");
  return 0;
}
