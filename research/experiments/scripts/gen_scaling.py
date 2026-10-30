#!/usr/bin/env python3
"""Generate single-TU scaling programs: a DAG of N interface/implementation
pairs wired by each DI framework.

Graph (identical for every framework, deterministic):
  * component k (0 <= k < N): interface I<k> with `virtual int value() const`,
    implementation C<k> final : I<k>.
  * C<k>'s constructor takes std::shared_ptr<I<d>> for each dependency d:
    always k-1 (so everything is reachable from the root) plus up to two more
    earlier components chosen with a fixed LCG -> 1..3 constructor parameters.
  * every component is shared (one instance per container); Root depends on
    I<N-1> and is resolved once. Each C<k> computes its value in the
    constructor, so main() prints a checksum that must match the manual one.

Usage: gen_scaling.py <framework> <N> <output.cpp>
Frameworks: manual boost_di fruit fruit_flat kangaru dingo hypodermic qlibs_di
"""
import sys

FRAMEWORKS = ("manual", "boost_di", "fruit", "fruit_flat", "kangaru", "dingo",
              "hypodermic", "qlibs_di")
MOD = 1000003


def dependencies(n):
    state = 12345
    deps = []
    for k in range(n):
        if k == 0:
            deps.append([])
            continue
        chosen = [k - 1]
        for _ in range(2):
            state = (state * 1103515245 + 12345) % (2 ** 31)
            if k - 1 > 0:
                d = state % (k - 1)
                if d not in chosen:
                    chosen.append(d)
        deps.append(sorted(chosen))
    return deps


def domain(n, deps):
    out = ["#include <cstdio>", "#include <memory>", "", "namespace gen {"]
    for k in range(n):
        params = ", ".join(f"std::shared_ptr<I{d}> d{d}" for d in deps[k])
        inits = " + ".join([str(k)] + [f"d{d}->value()" for d in deps[k]])
        members = "".join(f" std::shared_ptr<I{d}> m{d}_;" for d in deps[k])
        keep = "".join(f", m{d}_(d{d})" for d in deps[k])
        out.append(f"struct I{k} {{ virtual ~I{k}() = default; virtual int value() const = 0; }};")
        out.append(f"struct C{k} final : I{k} {{")
        out.append(f"  explicit C{k}({params}) : v_(({inits}) % {MOD}){keep} {{}}" if params
                   else f"  C{k}() : v_({k}) {{}}")
        out.append(f"  int value() const override {{ return v_; }}")
        out.append(f"  int v_;{members}")
        out.append("};")
    last = n - 1
    out.append(f"struct Root {{ explicit Root(std::shared_ptr<I{last}> top) : top_(std::move(top)) {{}}"
               f" int value() const {{ return top_->value(); }} std::shared_ptr<I{last}> top_; }};")
    out.append("}  // namespace gen")
    out.append("using namespace gen;")
    out.append("")
    return out


def wiring(fw, n, deps):
    last = n - 1
    o = []
    if fw == "manual":
        o.append("int main() {")
        for k in range(n):
            args = ", ".join(f"c{d}" for d in deps[k])
            o.append(f"  std::shared_ptr<I{k}> c{k} = std::make_shared<C{k}>({args});")
        o.append(f"  Root root(c{last});")
    elif fw == "boost_di":
        o.insert(0, "#include <boost/di.hpp>")
        o.append("namespace di = boost::di;")
        o.append("int main() {")
        o.append("  auto injector = di::make_injector(")
        o.append(",\n".join(f"    di::bind<I{k}>.to<C{k}>().in(di::singleton)" for k in range(n)))
        o.append("  );")
        o.append("  Root root = injector.create<Root>();")
    elif fw == "fruit":
        # Idiomatic Fruit: one component per class, installing its dependencies'
        # components (Fruit de-duplicates repeated installs).
        o.insert(0, "#include <fruit/fruit.h>")
        for k in range(n):
            o.append(f"fruit::Component<I{k}> getC{k}Component() {{")
            o.append("  return fruit::createComponent()")
            for d in deps[k]:
                o.append(f"      .install(getC{d}Component)")
            sig = ", ".join(f"std::shared_ptr<I{d}>" for d in deps[k])
            o.append(f"      .registerConstructor<C{k}({sig})>()")
            o.append(f"      .bind<I{k}, C{k}>();")
            o.append("}")
        o.append("fruit::Component<Root> getRootComponent() {")
        o.append(f"  return fruit::createComponent().install(getC{last}Component)"
                 f".registerConstructor<Root(std::shared_ptr<I{last}>)>();")
        o.append("}")
        o.append("int main() {")
        o.append("  fruit::Injector<Root> injector(getRootComponent);")
        o.append("  Root& root = injector.get<Root&>();")
    elif fw == "fruit_flat":
        # All bindings in a single component (same shape as one Boost.DI injector).
        o.insert(0, "#include <fruit/fruit.h>")
        o.append("fruit::Component<Root> getRootComponent() {")
        o.append("  return fruit::createComponent()")
        for k in range(n):
            sig = ", ".join(f"std::shared_ptr<I{d}>" for d in deps[k])
            o.append(f"      .registerConstructor<C{k}({sig})>().bind<I{k}, C{k}>()")
        o.append(f"      .registerConstructor<Root(std::shared_ptr<I{last}>)>();")
        o.append("}")
        o.append("int main() {")
        o.append("  fruit::Injector<Root> injector(getRootComponent);")
        o.append("  Root& root = injector.get<Root&>();")
    elif fw == "kangaru":
        o.insert(0, "#include <kangaru/kangaru.hpp>")
        for k in range(n):
            o.append(f"struct C{k}Service;")
            o.append(f"struct I{k}Service : kgr::abstract_shared_service<I{k}>, "
                     f"kgr::defaults_to<C{k}Service> {{}};")
            dl = ", ".join(f"I{d}Service" for d in deps[k])
            o.append(f"struct C{k}Service : kgr::shared_service<C{k}, kgr::dependency<{dl}>>, "
                     f"kgr::overrides<I{k}Service> {{}};")
        o.append(f"struct RootService : kgr::service<Root, kgr::dependency<I{last}Service>> {{}};")
        o.append("int main() {")
        o.append("  kgr::container container;")
        o.append("  Root root = container.service<RootService>();")
    elif fw == "dingo":
        o.insert(0, "#include <dingo/container.h>\n#include <dingo/storage/shared.h>\n"
                    "#include <dingo/storage/unique.h>")
        o.append("int main() {")
        o.append("  using namespace dingo;")
        o.append("  container<> c;")
        for k in range(n):
            o.append(f"  c.register_type<scope<shared>, storage<std::shared_ptr<C{k}>>, "
                     f"interfaces<I{k}>>();")
        o.append("  c.register_type<scope<unique>, storage<Root>>();")
        o.append("  Root root = c.resolve<Root>();")
    elif fw == "hypodermic":
        o.insert(0, "#include <Hypodermic/Hypodermic.h>")
        o.append("int main() {")
        o.append("  Hypodermic::ContainerBuilder builder;")
        for k in range(n):
            o.append(f"  builder.registerType<C{k}>().as<I{k}>().singleInstance();")
        o.append("  builder.registerType<Root>();")
        o.append("  auto container = builder.build();")
        o.append("  auto root_ptr = container->resolve<Root>();")
        o.append("  Root& root = *root_ptr;")
    elif fw == "qlibs_di":
        o.insert(0, "#include <di>")
        o.append("struct Scope {")
        for k in range(n):
            o.append(f"  std::shared_ptr<I{k}> p{k};")
        o.append("};")
        o.append("int main() {")
        o.append("  Scope s;")
        o.append("  auto injector = di::overload{")
        for k in range(n):
            o.append(f"    [&s](di::is<std::shared_ptr<I{k}>> auto t) -> std::shared_ptr<I{k}> {{")
            o.append(f"      if (!s.p{k}) s.p{k} = std::make_shared<C{k}>(di::make<C{k}>(t));")
            o.append(f"      return s.p{k};")
            o.append("    },")
        o.append("    [](auto t) -> decltype(auto) { return di::make(t); },")
        o.append("  };")
        o.append("  Root root = di::make<Root>(injector);")
    else:
        raise SystemExit(f"unknown framework {fw}")
    o.append('  std::printf("checksum=%d\\n", root.value());')
    o.append("  return 0;")
    o.append("}")
    return o


def generate(fw, n):
    deps = dependencies(n)
    lines = wiring(fw, n, deps)
    # includes produced by wiring() come first, then the shared domain
    head = [l for l in lines if l.startswith("#include")]
    body = [l for l in lines if not l.startswith("#include")]
    banner = [f"// Generated by gen_scaling.py: framework={fw} N={n}"]
    return "\n".join(banner + head + domain(n, deps) + body) + "\n"


MULTI_TU_FRAMEWORKS = ("manual", "fruit")


def generate_multi_tu(fw, n):
    """Same graph, split the way Fruit is meant to be used: one translation
    unit per component plus main.cpp. Returns {filename: content}.
    Only manual and Fruit are supported (Boost.DI/Kangaru/dingo/qlibs need the
    concrete constructors visible where the graph is wired)."""
    deps = dependencies(n)
    last = n - 1
    hdr = ["#pragma once", "#include <memory>"]
    if fw == "fruit":
        hdr.append("#include <fruit/fruit.h>")
    hdr.append("namespace gen {")
    for k in range(n):
        hdr.append(f"struct I{k} {{ virtual ~I{k}() = default; virtual int value() const = 0; }};")
        if fw == "fruit":
            hdr.append(f"fruit::Component<I{k}> getC{k}Component();")
        else:
            params = ", ".join(f"std::shared_ptr<I{d}>" for d in deps[k])
            hdr.append(f"std::shared_ptr<I{k}> makeC{k}({params});")
    hdr.append(f"struct Root {{ explicit Root(std::shared_ptr<I{last}> top) : top_(std::move(top)) {{}}"
               f" int value() const {{ return top_->value(); }} std::shared_ptr<I{last}> top_; }};")
    hdr.append("}  // namespace gen")
    files = {"gen.hpp": "\n".join(hdr) + "\n"}
    for k in range(n):
        params = ", ".join(f"std::shared_ptr<I{d}> d{d}" for d in deps[k])
        inits = " + ".join([str(k)] + [f"d{d}->value()" for d in deps[k]])
        members = "".join(f" std::shared_ptr<I{d}> m{d}_;" for d in deps[k])
        keep = "".join(f", m{d}_(d{d})" for d in deps[k])
        o = [f'#include "gen.hpp"', "namespace gen {", "namespace {",
             f"struct C{k} final : I{k} {{",
             (f"  explicit C{k}({params}) : v_(({inits}) % {MOD}){keep} {{}}" if params
              else f"  C{k}() : v_({k}) {{}}"),
             "  int value() const override { return v_; }",
             f"  int v_;{members}", "};", "}  // namespace"]
        if fw == "fruit":
            o.append(f"fruit::Component<I{k}> getC{k}Component() {{")
            o.append("  return fruit::createComponent()")
            for d in deps[k]:
                o.append(f"      .install(getC{d}Component)")
            sig = ", ".join(f"std::shared_ptr<I{d}>" for d in deps[k])
            o.append(f"      .registerConstructor<C{k}({sig})>()")
            o.append(f"      .bind<I{k}, C{k}>();")
            o.append("}")
        else:
            args = ", ".join(f"std::move(d{d})" for d in deps[k])
            o.append(f"std::shared_ptr<I{k}> makeC{k}({params}) {{ return std::make_shared<C{k}>({args}); }}")
        o.append("}  // namespace gen")
        files[f"c{k}.cpp"] = "\n".join(o) + "\n"
    m = ['#include <cstdio>', '#include "gen.hpp"', "using namespace gen;"]
    if fw == "fruit":
        m.append("fruit::Component<Root> getRootComponent() {")
        m.append(f"  return fruit::createComponent().install(getC{last}Component)"
                 f".registerConstructor<Root(std::shared_ptr<I{last}>)>();")
        m.append("}")
        m.append("int main() {")
        m.append("  fruit::Injector<Root> injector(getRootComponent);")
        m.append("  Root& root = injector.get<Root&>();")
    else:
        m.append("int main() {")
        for k in range(n):
            args = ", ".join(f"c{d}" for d in deps[k])
            m.append(f"  std::shared_ptr<I{k}> c{k} = makeC{k}({args});")
        m.append(f"  Root root(c{last});")
    m.append('  std::printf("checksum=%d\\n", root.value());')
    m.append("  return 0;")
    m.append("}")
    files["main.cpp"] = "\n".join(m) + "\n"
    return files


def expected_checksum(n):
    deps = dependencies(n)
    values = []
    for k in range(n):
        values.append((k + sum(values[d] for d in deps[k])) % MOD)
    return values[n - 1]


if __name__ == "__main__":
    if len(sys.argv) != 4 or sys.argv[1] not in FRAMEWORKS:
        raise SystemExit(__doc__)
    with open(sys.argv[3], "w") as f:
        f.write(generate(sys.argv[1], int(sys.argv[2])))
