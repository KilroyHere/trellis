// compile-fail: a class whose only constructor is private, two classes below two roots that need it through different request forms
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: trellis::error::E002<app::vault, ?trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: trellis::error::path<app::bank, ?app::teller>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex-msvc: trellis::error::E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::vault(?:,|$)
// expect-regex-msvc: trellis::error::path<app::bank, ?app::teller>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Discovery can't find vault's constructor. The explain pass reaches it from create<bank>()
// through teller, and raises discovery's own report, which is unique to the class: office needs a
// vault too, through auditor, by std::shared_ptr where teller takes a std::unique_ptr, and adds
// no error.
//
// Which root's path the error shows is the compiler's choice: the root whose explain pass it
// instantiates first. Clang goes depth first, in source order; GCC instantiates the shorter chain
// first. Both roots are two classes above the cause here, so both compilers show bank's.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace app {

class vault {
  vault() = default;
};
struct teller {
  explicit teller(std::unique_ptr<vault> safe) : safe_(std::move(safe)) {}
  std::unique_ptr<vault> safe_;
};
struct bank {
  explicit bank(teller front) : front_(std::move(front)) {}
  teller front_;
};
struct auditor {
  explicit auditor(std::shared_ptr<vault> safe) : safe_(std::move(safe)) {}
  std::shared_ptr<vault> safe_;
};
struct office {
  explicit office(auditor checks) : checks_(std::move(checks)) {}
  auditor checks_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::vault>);
  [[maybe_unused]] const app::bank first = injector.create<app::bank>();
  [[maybe_unused]] const app::office second = injector.create<app::office>();
}
