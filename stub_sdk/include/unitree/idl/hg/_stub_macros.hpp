// Stub unitree_sdk2: IDL field macro.
//
// The real CycloneDDS-generated IDL classes expose every field through a
// method-call accessor that returns a mutable reference, e.g.
//   float& q();  const float& q() const;
// so call sites read/write as ``obj.q()`` / ``obj.q() = v``. The binding code in
// ``src/bindings.cpp`` is written against that style, so the stub must mirror it
// exactly; then one ``bindings.cpp`` compiles unchanged against both the real
// SDK and this stub.
//
// UT_FIELD is variadic so a type containing a comma (e.g. ``std::array<T, N>``)
// passes through as a single field type.
#pragma once

#define UT_FIELD(NAME, ...)                            \
  __VA_ARGS__ NAME##_{};                               \
  __VA_ARGS__ &NAME() { return NAME##_; }              \
  const __VA_ARGS__ &NAME() const { return NAME##_; }
