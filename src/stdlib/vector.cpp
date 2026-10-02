#include "stdlib/vector.h"
#include "stdlib/random.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

/* the C++ half of `yok vector;`, ported from keo's math/vec.h (same
 * formulas, double instead of float). VECTOR_SOURCE at the bottom declares
 * Vec2 / Vec3 / Vec4 and every operation as a body-less rupamun, the
 * compiler binds each one to the function here with the same signature.
 *
 * a VecN is N consecutive LekThom slots. args come flattened in parameter
 * order (a method's receiver first), results go into `out` */

namespace steav_stdlib {

using steav_vm::Value;
using steav_vm::make_bool;
using steav_vm::make_lek_kut;
using steav_vm::make_lek_thom;

namespace {

template <int N> struct V {
  double c[N];
};

template <int N> inline V<N> load(const Value *s) {
  V<N> v;
  for (int i = 0; i < N; i++) v.c[i] = s[i].lek_thom;
  return v;
}

template <int N> inline void store(const V<N> &v, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(v.c[i]);
}

template <int N> inline double dot_of(const V<N> &a, const V<N> &b) {
  double sum = 0;
  for (int i = 0; i < N; i++) sum += a.c[i] * b.c[i];
  return sum;
}

inline V<3> v3(double x, double y, double z) {
  V<3> v = {{x, y, z}};
  return v;
}

/* ------------------------------------------------------ VecN op VecN / s */

template <int N> void add(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom + a[N + i].lek_thom);
}
template <int N> void sub(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom - a[N + i].lek_thom);
}
template <int N> void mul(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom * a[N + i].lek_thom);
}
template <int N> void div(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom / a[N + i].lek_thom);
}

template <int N> void add_s(const Value *a, Value *out) {
  double s = a[N].lek_thom;
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom + s);
}
template <int N> void sub_s(const Value *a, Value *out) {
  double s = a[N].lek_thom;
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom - s);
}
template <int N> void mul_s(const Value *a, Value *out) {
  double s = a[N].lek_thom;
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom * s);
}
// keo multiplies by the inverse
template <int N> void div_s(const Value *a, Value *out) {
  double inv = 1.0 / a[N].lek_thom;
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[i].lek_thom * inv);
}
// s * v
template <int N> void s_mul(const Value *a, Value *out) {
  double s = a[0].lek_thom;
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(a[1 + i].lek_thom * s);
}

template <int N> void neg(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(-a[i].lek_thom);
}
template <int N> void eq(const Value *a, Value *out) {
  bool same = true;
  for (int i = 0; i < N; i++) same = same && a[i].lek_thom == a[N + i].lek_thom;
  out[0] = make_bool(same);
}
template <int N> void ne(const Value *a, Value *out) {
  eq<N>(a, out);
  out[0].boolean = !out[0].boolean;
}

/* ------------------------------------------------------------- methods */

template <int N> void length_sq(const Value *a, Value *out) {
  V<N> v = load<N>(a);
  out[0] = make_lek_thom(dot_of(v, v));
}
template <int N> void length(const Value *a, Value *out) {
  V<N> v = load<N>(a);
  out[0] = make_lek_thom(sqrt(dot_of(v, v)));
}
template <int N> void normalized(const Value *a, Value *out) {
  V<N> v = load<N>(a);
  double inv = 1.0 / sqrt(dot_of(v, v));
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(v.c[i] * inv);
}
template <int N> void vmin(const Value *a, Value *out) {
  for (int i = 0; i < N; i++)
    out[i] = make_lek_thom(std::min(a[i].lek_thom, a[N + i].lek_thom));
}
template <int N> void vmax(const Value *a, Value *out) {
  for (int i = 0; i < N; i++)
    out[i] = make_lek_thom(std::max(a[i].lek_thom, a[N + i].lek_thom));
}
template <int N> void vabs(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(fabs(a[i].lek_thom));
}

/* -------------------------------------------------- casts, VecN <-> VecNi */

// same semantics as the scalar LekKut(x) cast: truncates toward zero,
// clamped (not wrapped) at the 32 bit boundary, NaN -> 0
inline int32_t to_lekkut_component(double v) {
  if (std::isnan(v)) return 0;
  if (v > (double)INT32_MAX) return INT32_MAX;
  if (v < (double)INT32_MIN) return INT32_MIN;
  return (int32_t)v;
}

template <int N> void to_lekkut(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_kut(to_lekkut_component(a[i].lek_thom));
}
// LekKut -> LekThom is always exact, no clamping needed
template <int N> void to_lekthom(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom((double)a[i].lek_kut);
}

void near_zero(const Value *a, Value *out) {
  const double eps = 1e-8;
  out[0] = make_bool(fabs(a[0].lek_thom) < eps && fabs(a[1].lek_thom) < eps &&
                     fabs(a[2].lek_thom) < eps);
}
void xyz(const Value *a, Value *out) {
  for (int i = 0; i < 3; i++) out[i] = a[i];
}

/* ------------------------------------------------------ free functions */

template <int N> void dot(const Value *a, Value *out) {
  out[0] = make_lek_thom(dot_of(load<N>(a), load<N>(a + N)));
}
template <int N> void lerp(const Value *a, Value *out) {
  double t = a[2 * N].lek_thom;
  for (int i = 0; i < N; i++) {
    double from = a[i].lek_thom;
    out[i] = make_lek_thom(from + (a[N + i].lek_thom - from) * t);
  }
}

inline V<3> cross_of(const V<3> &a, const V<3> &b) {
  return v3(a.c[1] * b.c[2] - a.c[2] * b.c[1], a.c[2] * b.c[0] - a.c[0] * b.c[2],
            a.c[0] * b.c[1] - a.c[1] * b.c[0]);
}
void cross(const Value *a, Value *out) { store(cross_of(load<3>(a), load<3>(a + 3)), out); }
void cross2(const Value *a, Value *out) {
  out[0] = make_lek_thom(a[0].lek_thom * a[3].lek_thom - a[1].lek_thom * a[2].lek_thom);
}

// Rodrigues' rotation formula, rotate v around axis by angle (radians)
void rotate(const Value *a, Value *out) {
  V<3> v = load<3>(a);
  V<3> axis = load<3>(a + 3);
  double angle = a[6].lek_thom;
  double c = cos(angle);
  double s = sin(angle);
  double inv = 1.0 / sqrt(dot_of(axis, axis));
  V<3> k = v3(axis.c[0] * inv, axis.c[1] * inv, axis.c[2] * inv);
  V<3> kxv = cross_of(k, v);
  double kdv = dot_of(k, v) * (1.0 - c);
  V<3> r;
  for (int i = 0; i < 3; i++) r.c[i] = v.c[i] * c + kxv.c[i] * s + k.c[i] * kdv;
  store(r, out);
}

// reflect v around normal n
void reflect(const Value *a, Value *out) {
  V<3> v = load<3>(a);
  V<3> n = load<3>(a + 3);
  double d = 2.0 * dot_of(v, n);
  for (int i = 0; i < 3; i++) out[i] = make_lek_thom(v.c[i] - d * n.c[i]);
}

// snell's law
void refract(const Value *a, Value *out) {
  V<3> uv = load<3>(a);
  V<3> n = load<3>(a + 3);
  double etai_over_etat = a[6].lek_thom;
  V<3> neg_uv = v3(-uv.c[0], -uv.c[1], -uv.c[2]);
  double cos_theta = std::min(dot_of(neg_uv, n), 1.0);
  V<3> r_perp;
  for (int i = 0; i < 3; i++) r_perp.c[i] = etai_over_etat * (uv.c[i] + cos_theta * n.c[i]);
  double par = -sqrt(fabs(1.0 - dot_of(r_perp, r_perp)));
  for (int i = 0; i < 3; i++) out[i] = make_lek_thom(r_perp.c[i] + par * n.c[i]);
}

// schlick fresnel approximation
void schlick_approx(const Value *a, Value *out) {
  double cosine = a[0].lek_thom;
  double ref_idx = a[1].lek_thom;
  double r0 = (1.0 - ref_idx) / (1.0 + ref_idx);
  r0 = r0 * r0;
  out[0] = make_lek_thom(r0 + (1.0 - r0) * pow(1.0 - cosine, 5.0));
}

// keo's static zero() / one() / up() / right() / forward()
template <int N> void zero(const Value *, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(0);
}
template <int N> void one(const Value *, Value *out) {
  for (int i = 0; i < N; i++) out[i] = make_lek_thom(1);
}
void up(const Value *, Value *out) { store(v3(0, 1, 0), out); }
void right(const Value *, Value *out) { store(v3(1, 0, 0), out); }
void forward(const Value *, Value *out) { store(v3(0, 0, -1), out); }

// keo's explicit VecN(float s)
template <int N> void splat(const Value *a, Value *out) {
  for (int i = 0; i < N; i++) out[i] = a[0];
}

// keo's Vec4(const Vec3 &v, float w)
void extend(const Value *a, Value *out) {
  for (int i = 0; i < 4; i++) out[i] = a[i];
}

/* ------------------------------------------ random helpers, keo's PCG32 */

inline V<3> random_unit_vector_of() {
  PCG32 &rng = shared_rng();
  while (true) {
    V<3> p = v3(rng.random_interval(-1.0f, 1.0f), rng.random_interval(-1.0f, 1.0f),
                rng.random_interval(-1.0f, 1.0f));
    double sq = dot_of(p, p);
    if (1e-7 < sq && sq <= 1.0) {
      double inv = 1.0 / sqrt(sq);
      return v3(p.c[0] * inv, p.c[1] * inv, p.c[2] * inv);
    }
  }
}

void random_unit_vector(const Value *, Value *out) { store(random_unit_vector_of(), out); }

void random_in_unit_disk(const Value *, Value *out) {
  PCG32 &rng = shared_rng();
  while (true) {
    V<3> p = v3(rng.random_interval(-1.0f, 1.0f), rng.random_interval(-1.0f, 1.0f), 0.0);
    if (dot_of(p, p) < 1.0) {
      store(p, out);
      return;
    }
  }
}

void random_on_hemisphere(const Value *a, Value *out) {
  V<3> on_unit_sphere = random_unit_vector_of();
  if (dot_of(on_unit_sphere, load<3>(a)) <= 0) {
    for (int i = 0; i < 3; i++) on_unit_sphere.c[i] = -on_unit_sphere.c[i];
  }
  store(on_unit_sphere, out);
}

} // namespace

/* what the type checker sees: the sampoans + every signature, no bodies.
 * each `rupamun ...;` gets bound to the C++ with the same signature in
 * VECTOR_BOUND below */
extern const char VECTOR_SOURCE[];
const char VECTOR_SOURCE[] = R"STEAVSTS(// vector: Vec2 / Vec3 / Vec4, ported from keo's math/vec.h.
//
// this file only DECLARES things: the sampoans (so v.x, printing and
// storing them inline on the stack work like any struct) and every
// operation as a `rupamun ...;` with no body. the bodies are C++ in
// VECTOR_BOUND below, compiled into the binary and bound by signature.
//
// no overloading by name in v1, so the Vec3 version of a free function
// gets keo's plain name, Vec2 / Vec4 get a 2 / 4 suffix (dot / dot2 / dot4).
// keo's min / max / abs are methods (v.min(u)) so they don't clash with
// math's scalar ones.

sampoan Vec2 {
    akthe x: LekThom;
    akthe y: LekThom;

    rupamun length_sq() -> LekThom;
    rupamun length() -> LekThom;
    rupamun normalized() -> Vec2;
    rupamun min(o: Vec2) -> Vec2;
    rupamun max(o: Vec2) -> Vec2;
    rupamun abs() -> Vec2;
}

rupamun +(a: Vec2, b: Vec2) -> Vec2;
rupamun -(a: Vec2, b: Vec2) -> Vec2;
rupamun *(a: Vec2, b: Vec2) -> Vec2;
rupamun /(a: Vec2, b: Vec2) -> Vec2;
rupamun +(a: Vec2, s: LekThom) -> Vec2;
rupamun -(a: Vec2, s: LekThom) -> Vec2;
rupamun *(a: Vec2, s: LekThom) -> Vec2;
rupamun /(a: Vec2, s: LekThom) -> Vec2;
rupamun *(s: LekThom, a: Vec2) -> Vec2;
rupamun -(a: Vec2) -> Vec2;
rupamun ==(a: Vec2, b: Vec2) -> Boolean;
rupamun !=(a: Vec2, b: Vec2) -> Boolean;

sampoan Vec3 {
    akthe x: LekThom;
    akthe y: LekThom;
    akthe z: LekThom;

    rupamun length_sq() -> LekThom;
    rupamun length() -> LekThom;
    rupamun normalized() -> Vec3;
    rupamun min(o: Vec3) -> Vec3;
    rupamun max(o: Vec3) -> Vec3;
    rupamun abs() -> Vec3;
    rupamun near_zero() -> Boolean;
}

rupamun +(a: Vec3, b: Vec3) -> Vec3;
rupamun -(a: Vec3, b: Vec3) -> Vec3;
rupamun *(a: Vec3, b: Vec3) -> Vec3;
rupamun /(a: Vec3, b: Vec3) -> Vec3;
rupamun +(a: Vec3, s: LekThom) -> Vec3;
rupamun -(a: Vec3, s: LekThom) -> Vec3;
rupamun *(a: Vec3, s: LekThom) -> Vec3;
rupamun /(a: Vec3, s: LekThom) -> Vec3;
rupamun *(s: LekThom, a: Vec3) -> Vec3;
rupamun -(a: Vec3) -> Vec3;
rupamun ==(a: Vec3, b: Vec3) -> Boolean;
rupamun !=(a: Vec3, b: Vec3) -> Boolean;

sampoan Vec4 {
    akthe x: LekThom;
    akthe y: LekThom;
    akthe z: LekThom;
    akthe w: LekThom;

    rupamun length_sq() -> LekThom;
    rupamun length() -> LekThom;
    rupamun normalized() -> Vec4;
    rupamun min(o: Vec4) -> Vec4;
    rupamun max(o: Vec4) -> Vec4;
    rupamun abs() -> Vec4;
    rupamun xyz() -> Vec3;
}

rupamun +(a: Vec4, b: Vec4) -> Vec4;
rupamun -(a: Vec4, b: Vec4) -> Vec4;
rupamun *(a: Vec4, b: Vec4) -> Vec4;
rupamun /(a: Vec4, b: Vec4) -> Vec4;
rupamun +(a: Vec4, s: LekThom) -> Vec4;
rupamun -(a: Vec4, s: LekThom) -> Vec4;
rupamun *(a: Vec4, s: LekThom) -> Vec4;
rupamun /(a: Vec4, s: LekThom) -> Vec4;
rupamun *(s: LekThom, a: Vec4) -> Vec4;
rupamun -(a: Vec4) -> Vec4;
rupamun ==(a: Vec4, b: Vec4) -> Boolean;
rupamun !=(a: Vec4, b: Vec4) -> Boolean;

// LekKut-component counterparts, cast to/from with to_lekkut(2/4) / to_lekthom(2/4)
sampoan Vec2i {
    akthe x: LekKut;
    akthe y: LekKut;
}

sampoan Vec3i {
    akthe x: LekKut;
    akthe y: LekKut;
    akthe z: LekKut;
}

sampoan Vec4i {
    akthe x: LekKut;
    akthe y: LekKut;
    akthe z: LekKut;
    akthe w: LekKut;
}

// componentwise casts: to_lekkut truncates toward zero and clamps at the
// 32 bit boundary, same as the scalar LekKut(x) cast; to_lekthom is exact
rupamun to_lekkut(v: Vec3) -> Vec3i;
rupamun to_lekthom(v: Vec3i) -> Vec3;
rupamun to_lekkut2(v: Vec2) -> Vec2i;
rupamun to_lekthom2(v: Vec2i) -> Vec2;
rupamun to_lekkut4(v: Vec4) -> Vec4i;
rupamun to_lekthom4(v: Vec4i) -> Vec4;

rupamun dot(a: Vec3, b: Vec3) -> LekThom;
rupamun dot2(a: Vec2, b: Vec2) -> LekThom;
rupamun dot4(a: Vec4, b: Vec4) -> LekThom;
rupamun lerp(a: Vec3, b: Vec3, t: LekThom) -> Vec3;
rupamun lerp2(a: Vec2, b: Vec2, t: LekThom) -> Vec2;
rupamun lerp4(a: Vec4, b: Vec4, t: LekThom) -> Vec4;

// Vec3 x Vec3 is a Vec3, Vec2 x Vec2 is the z of that (a number)
rupamun cross(a: Vec3, b: Vec3) -> Vec3;
rupamun cross2(a: Vec2, b: Vec2) -> LekThom;

// Rodrigues' rotation formula, rotate v around axis by angle (radians).
// PI_4 / PI_2 / PI / TAU from math for 45 / 90 / 180 / 360 degrees
rupamun rotate(v: Vec3, axis: Vec3, angle: LekThom) -> Vec3;

// reflect v around normal n
rupamun reflect(v: Vec3, n: Vec3) -> Vec3;
// snell's law
rupamun refract(uv: Vec3, n: Vec3, etai_over_etat: LekThom) -> Vec3;
// schlick fresnel approximation
rupamun schlick_approx(cosine: LekThom, ref_idx: LekThom) -> LekThom;

// keo's static zero() / one() / up() / right() / forward()
rupamun zero() -> Vec3;
rupamun one() -> Vec3;
rupamun up() -> Vec3;
rupamun right() -> Vec3;
rupamun forward() -> Vec3;
rupamun zero2() -> Vec2;
rupamun one2() -> Vec2;
rupamun zero4() -> Vec4;
rupamun one4() -> Vec4;

// keo's explicit Vec3(float s), every component = s
rupamun splat(s: LekThom) -> Vec3;
rupamun splat2(s: LekThom) -> Vec2;
rupamun splat4(s: LekThom) -> Vec4;

// keo's Vec4(const Vec3 &v, float w)
rupamun extend(v: Vec3, w: LekThom) -> Vec4;

// keo's random helpers, on the same PCG32 as `yok random`
rupamun random_unit_vector() -> Vec3;
rupamun random_in_unit_disk() -> Vec3;
rupamun random_on_hemisphere(normal: Vec3) -> Vec3;
)STEAVSTS";

/* signature -> C++, has to match the declarations in VECTOR_SOURCE
 * exactly (a missing one is a compile error that names it) */
#define STEAV_VEC_OPS(N, T)                                                    \
  {"+(" T "," T ")", add<N>}, {"-(" T "," T ")", sub<N>},                      \
      {"*(" T "," T ")", mul<N>}, {"/(" T "," T ")", div<N>},                  \
      {"+(" T ",LekThom)", add_s<N>}, {"-(" T ",LekThom)", sub_s<N>},          \
      {"*(" T ",LekThom)", mul_s<N>}, {"/(" T ",LekThom)", div_s<N>},          \
      {"*(LekThom," T ")", s_mul<N>}, {"-(" T ")", neg<N>},                    \
      {"==(" T "," T ")", eq<N>}, {"!=(" T "," T ")", ne<N>},                  \
      {T ".length_sq()", length_sq<N>}, {T ".length()", length<N>},            \
      {T ".normalized()", normalized<N>}, {T ".min(" T ")", vmin<N>},          \
      {T ".max(" T ")", vmax<N>}, {T ".abs()", vabs<N>}

static const BoundFn VECTOR_BOUND[] = {
    STEAV_VEC_OPS(2, "Vec2"),
    STEAV_VEC_OPS(3, "Vec3"),
    STEAV_VEC_OPS(4, "Vec4"),
    {"Vec3.near_zero()", near_zero},
    {"Vec4.xyz()", xyz},

    {"to_lekkut(Vec3)", to_lekkut<3>},
    {"to_lekthom(Vec3i)", to_lekthom<3>},
    {"to_lekkut2(Vec2)", to_lekkut<2>},
    {"to_lekthom2(Vec2i)", to_lekthom<2>},
    {"to_lekkut4(Vec4)", to_lekkut<4>},
    {"to_lekthom4(Vec4i)", to_lekthom<4>},

    {"dot(Vec3,Vec3)", dot<3>},
    {"dot2(Vec2,Vec2)", dot<2>},
    {"dot4(Vec4,Vec4)", dot<4>},
    {"lerp(Vec3,Vec3,LekThom)", lerp<3>},
    {"lerp2(Vec2,Vec2,LekThom)", lerp<2>},
    {"lerp4(Vec4,Vec4,LekThom)", lerp<4>},
    {"cross(Vec3,Vec3)", cross},
    {"cross2(Vec2,Vec2)", cross2},
    {"rotate(Vec3,Vec3,LekThom)", rotate},
    {"reflect(Vec3,Vec3)", reflect},
    {"refract(Vec3,Vec3,LekThom)", refract},
    {"schlick_approx(LekThom,LekThom)", schlick_approx},

    {"zero()", zero<3>},
    {"one()", one<3>},
    {"up()", up},
    {"right()", right},
    {"forward()", forward},
    {"zero2()", zero<2>},
    {"one2()", one<2>},
    {"zero4()", zero<4>},
    {"one4()", one<4>},
    {"splat(LekThom)", splat<3>},
    {"splat2(LekThom)", splat<2>},
    {"splat4(LekThom)", splat<4>},
    {"extend(Vec3,LekThom)", extend},

    {"random_unit_vector()", random_unit_vector},
    {"random_in_unit_disk()", random_in_unit_disk},
    {"random_on_hemisphere(Vec3)", random_on_hemisphere},
};
#undef STEAV_VEC_OPS

const Module VECTOR_MODULE = {
    "vector",
    nullptr,
    0,
    nullptr,
    0,
    VECTOR_SOURCE,
    VECTOR_BOUND,
    (int)(sizeof(VECTOR_BOUND) / sizeof(VECTOR_BOUND[0])),
};

} // namespace steav_stdlib
