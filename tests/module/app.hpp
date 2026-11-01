// Domain types for the module smoke tests (tests/module/), shared by the translation unit that
// imports trellis and the one that includes its headers. Like any domain class (ADR-0007), they
// know nothing of Trellis.
#ifndef TRELLIS_TESTS_MODULE_APP_HPP
#define TRELLIS_TESTS_MODULE_APP_HPP

namespace mod {

class clock_source {
 public:
  clock_source() = default;
  clock_source(const clock_source&) = delete;
  clock_source& operator=(const clock_source&) = delete;
  virtual ~clock_source() = default;
  [[nodiscard]] virtual long now() const = 0;
};

class system_clock final : public clock_source {
 public:
  [[nodiscard]] long now() const override { return 42; }
};

struct config {
  int port = 0;
};

class service {
 public:
  explicit service(int port) : port_(port) {}
  [[nodiscard]] int port() const { return port_; }

 private:
  int port_;
};

}  // namespace mod

#endif  // TRELLIS_TESTS_MODULE_APP_HPP
