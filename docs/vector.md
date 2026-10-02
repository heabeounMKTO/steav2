# `vector`

`Vec2`, `Vec3`, `Vec4` — ported from keo's `math/vec.h`, `LekThom` (double)
throughout instead of `float`, same formulas. Native C++
(`src/stdlib/vector.cpp`), bound the same way every stdlib module is (see
[modules.md](modules.md)): the module declares each operation as a
body-less `rupamun ...;` and the compiler binds it to the matching C++
function by signature, so a call is a single `OP_CALL` with no bytecode
frame pushed.

```
yok vector;          // Vec2/Vec3/Vec4, dot, cross, ... in scope unqualified
yok vector jea vec;  // vec.Vec2, vec.dot, ... instead
```

`Vec2`/`Vec3`/`Vec4` are ordinary `sampoan`s to the language — `v.x`,
printing, memberwise construction (`Vec3(1, 2, 3)`), stored inline on the
value stack, never on the heap. Nothing here is special-cased in the VM;
`vector` just happens to be the stdlib module that declares them.

steav2 has no overloading by name, so **the `Vec3` version of a free
function gets keo's plain name, and the `Vec2` / `Vec4` versions get a
numeric suffix** — `dot`/`dot2`/`dot4`, `lerp`/`lerp2`/`lerp4`, and so on,
throughout this page. keo's `min`/`max`/`abs` are methods here, so they
don't clash with `math`'s scalar free functions of the same name. keo's
mutating `normalize()`, `+=`, and `v[i]` aren't ported (no compound
assignment overloads or struct indexing into a `sampoan`'s own fields in
v1) — write `v = v.normalized();` instead.

## Types

```
sampoan Vec2 { akthe x: LekThom; akthe y: LekThom; }
sampoan Vec3 { akthe x: LekThom; akthe y: LekThom; akthe z: LekThom; }
sampoan Vec4 { akthe x: LekThom; akthe y: LekThom; akthe z: LekThom; akthe w: LekThom; }
```

Construction is the same free memberwise construction any `sampoan` gets —
`Vec3(1, 2, 3)`, no method needed, arguments in field-declaration order.

### `Vec2i` / `Vec3i` / `Vec4i`

`LekKut`-component counterparts — same field names, no operators or
methods of their own, just something to cast to and from (the canonical
use: rounding a computed position down to a pixel index for a framebuffer
or image write):

```
rupamun to_lekkut(v: Vec3) -> Vec3i;   // truncates toward zero, clamped
rupamun to_lekthom(v: Vec3i) -> Vec3;  // (not wrapped) at the 32 bit
rupamun to_lekkut2(v: Vec2) -> Vec2i;  // boundary, same as the scalar
rupamun to_lekthom2(v: Vec2i) -> Vec2; // LekKut(x) cast; to_lekthom* is
rupamun to_lekkut4(v: Vec4) -> Vec4i;  // always exact
rupamun to_lekthom4(v: Vec4i) -> Vec4;
```

```
yok vector;
akthe p: Vec2 = Vec2(103.7, 44.2);
akthe pixel = to_lekkut2(p);
jongyeytha pixel; // Vec2i(103, 44)
```

## Operators

Per type, `VecN` standing for whichever of `Vec2`/`Vec3`/`Vec4` — each is
declared separately per type, there's no generic "for any VecN" mechanism
in the language itself:

```
rupamun +(a: VecN, b: VecN) -> VecN;    // componentwise
rupamun -(a: VecN, b: VecN) -> VecN;    // componentwise
rupamun *(a: VecN, b: VecN) -> VecN;    // componentwise
rupamun /(a: VecN, b: VecN) -> VecN;    // componentwise
rupamun +(a: VecN, s: LekThom) -> VecN;
rupamun -(a: VecN, s: LekThom) -> VecN;
rupamun *(a: VecN, s: LekThom) -> VecN;
rupamun /(a: VecN, s: LekThom) -> VecN; // keo multiplies by 1/s
rupamun *(s: LekThom, a: VecN) -> VecN; // declared separately, no auto-commutativity
rupamun -(a: VecN) -> VecN;             // unary negate
rupamun ==(a: VecN, b: VecN) -> Boolean;
rupamun !=(a: VecN, b: VecN) -> Boolean;
```

`v * v` is componentwise, not a dot product — use `dot(a, b)` for that.

## Methods

Identical shape on all three (each returns its own type unless noted):

```
rupamun length_sq() -> LekThom;
rupamun length() -> LekThom;
rupamun normalized() -> VecN;
rupamun min(o: VecN) -> VecN;
rupamun max(o: VecN) -> VecN;
rupamun abs() -> VecN;
```

Plus two that only make sense on one arity:

```
rupamun Vec3.near_zero() -> Boolean; // every component within 1e-8 of 0
rupamun Vec4.xyz() -> Vec3;          // drop w
```

## Free functions

```
rupamun dot(a: Vec3, b: Vec3) -> LekThom;
rupamun dot2(a: Vec2, b: Vec2) -> LekThom;
rupamun dot4(a: Vec4, b: Vec4) -> LekThom;

rupamun lerp(a: Vec3, b: Vec3, t: LekThom) -> Vec3;
rupamun lerp2(a: Vec2, b: Vec2, t: LekThom) -> Vec2;
rupamun lerp4(a: Vec4, b: Vec4, t: LekThom) -> Vec4;

rupamun cross(a: Vec3, b: Vec3) -> Vec3;
rupamun cross2(a: Vec2, b: Vec2) -> LekThom;  // 2D cross is a scalar (the z you'd get embedding both in 3D)
```

Geometry, all `Vec3`:

```
// Rodrigues' rotation formula: rotate v around axis by angle (radians).
// math's PI_4 / PI_2 / PI / TAU are 45 / 90 / 180 / 360 degrees
rupamun rotate(v: Vec3, axis: Vec3, angle: LekThom) -> Vec3;

rupamun reflect(v: Vec3, n: Vec3) -> Vec3;                          // reflect v around normal n
rupamun refract(uv: Vec3, n: Vec3, etai_over_etat: LekThom) -> Vec3; // Snell's law
rupamun schlick_approx(cosine: LekThom, ref_idx: LekThom) -> LekThom; // Schlick's Fresnel approximation
```

Constants and construction helpers:

```
rupamun zero() -> Vec3;    rupamun zero2() -> Vec2;   rupamun zero4() -> Vec4;
rupamun one() -> Vec3;     rupamun one2() -> Vec2;    rupamun one4() -> Vec4;
rupamun up() -> Vec3;       // (0, 1, 0)
rupamun right() -> Vec3;    // (1, 0, 0)
rupamun forward() -> Vec3;  // (0, 0, -1)

rupamun splat(s: LekThom) -> Vec3;   // keo's explicit VecN(float s): every component = s
rupamun splat2(s: LekThom) -> Vec2;
rupamun splat4(s: LekThom) -> Vec4;

rupamun extend(v: Vec3, w: LekThom) -> Vec4; // keo's Vec4(const Vec3 &v, float w)
```

Random sampling, keo's PCG32 (same RNG [`random`](modules.md#random) uses
— `random.seed(s)` reseeds these too):

```
rupamun random_unit_vector() -> Vec3;
rupamun random_in_unit_disk() -> Vec3;         // z = 0
rupamun random_on_hemisphere(normal: Vec3) -> Vec3; // random_unit_vector(), flipped onto normal's side
```

**Not part of `vector`:** `Color` is not a generic linear-algebra type, so
it isn't shipped here — it lives in the ray-tracing implementation itself,
where it can grow renderer-specific behavior (gamma correction, clamping,
`toRGB8()`) that a generic vector module shouldn't have to carry.

## Examples

### Normal, reflection, and a diffuse bounce

```
bongSlanhOun "vector example";
yok vector;

akthe n = up();                          // surface normal
akthe incoming = Vec3(1, -1, 0).normalized();
jongyeytha reflect(incoming, n);          // Vec3(0.7071067811865475, 0.7071067811865475, 0)

// a Lambertian bounce direction: a random point on the hemisphere around n
yok random;
seed(1);                                    // random itself, not `random.seed` — a bare `yok` puts it in scope unqualified
jongyeytha random_on_hemisphere(n).length(); // 1: it's always a unit vector
```

### Basis vectors and orthogonality

```
yok vector;
yok math jea m;
jongyeytha dot(cross(right(), up()), forward()); // -1: cross(right, up) = (0, 0, 1) = -forward() here

// rotating right() 90 degrees around up() lands on forward() (up to fp noise) —
// near_zero() checks a vector against (0, 0, 0), so difference it from
// forward() first, not the rotated vector on its own
akthe swung = rotate(right(), up(), m.PI_2);
jongyeytha swung;                          // Vec3(6.123233995736766e-17, 0, -1)
jongyeytha (swung - forward()).near_zero(); // ok
```

### Blending two colors

```
yok vector;
akthe warm = Vec3(1.0, 0.6, 0.2);
akthe cool = Vec3(0.1, 0.3, 0.9);
akthe mixed = lerp(warm, cool, 0.5);
jongyeytha mixed; // Vec3(0.55, 0.44999999999999996, 0.55)
```

### Casting a computed position to a pixel index

```
yok vector;
akthe camera_space: Vec2 = Vec2(319.6, 240.1);
akthe pixel = to_lekkut2(camera_space); // Vec2i(319, 240), ready to index a framebuffer row/col
jongyeytha pixel;
```

See [`examples/sts/tour.sts`](../examples/sts/tour.sts) for `vector`
alongside the rest of the language, and
[`examples/sts/basics.sts`](../examples/sts/basics.sts) for the control
flow / loops / functions used to build up to these.
