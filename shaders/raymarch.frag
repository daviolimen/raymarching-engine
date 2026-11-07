#version 330 core

out vec4 fragColor;

uniform float uTime;
uniform vec2  uResolution;
uniform vec3  uCamPos;
uniform vec3  uCamTarget;
uniform float uFov;

uniform int   uMaxSteps;
uniform float uMaxDist;
uniform float uSurfEps;

uniform vec3  uLightDir;
uniform vec3  uLightColor;
uniform float uAmbient;
uniform float uSpecular;
uniform float uShininess;
uniform int   uShadows;
uniform float uShadowK;
uniform int   uAO;
uniform float uFogDensity;
uniform int   uGamma;
uniform int   uDebugView;

float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdPlane(vec3 p) {
    return p.y;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 b, float r) {
    return sdBox(p, b - r) - r;
}

float sdTorus(vec3 p, vec2 t) {
    return length(vec2(length(p.xz) - t.x, p.y)) - t.y;
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 closer = clamp(dot(p - a, b - a) / dot(b - a, b - a), 0.0, 1.0) * (b - a) + a;
    return length(p - closer) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 q = vec2(length(p.xz), abs(p.y)) - vec2(r, h);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}

float sdCappedCone(vec3 p, float h, float r1, float r2) {
    vec2 q = vec2(length(p.xz), p.y);
    vec2 k1 = vec2(r2, h);
    vec2 k2 = vec2(r2 - r1, 2.0 * h);
    vec2 ca = vec2(q.x - min(q.x, q.y < 0.0 ? r1 : r2), abs(q.y) - h);
    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}

float sdOctahedron(vec3 p, float s) {
    p = abs(p);
    return (p.x + p.y + p.z - s) * 0.57735027;
}

vec3 toLocal(vec3 p, vec3 pos, mat3 rot) {
    return transpose(rot) * (p - pos);
}

float smin(float a, float b, float k) {
    if (k == 0) return min(a, b);
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * k / 4.0;
}

float smax(float a, float b, float k) {
    return -smin(-a, -b, k);
}

float sminBlend(float a, float b, float k) {
    if (k == 0.0) return b < a ? 1.0 : 0.0;
    float x = a - b;
    float alpha = 1.0 / (4.0*k);
    return clamp(x > 0.0 ? 1.0 - 2.0*alpha*(k-abs(x)) : 2.0*alpha*(k-abs(x)), 0.0, 1.0);
}

void opUnion(inout float d, inout vec3 color, float dObj, vec3 colorObj) {
    if (dObj < d) {
        d = dObj;
        color = colorObj;
    }
}

const int MAX_OBJECTS = 32;

const int OBJ_SPHERE = 0, OBJ_BOX = 1, OBJ_ROUND_BOX = 2, OBJ_TORUS = 3,
          OBJ_CAPSULE = 4, OBJ_CYLINDER = 5, OBJ_CONE = 6, OBJ_OCTAHEDRON = 7;
const int OP_UNION = 0, OP_SUBTRACT = 1, OP_INTERSECT = 2;

uniform int   uObjectCount;
uniform int   uObjType[MAX_OBJECTS];
uniform int   uObjOp[MAX_OBJECTS];
uniform float uObjK[MAX_OBJECTS];
uniform vec3  uObjPos[MAX_OBJECTS];
uniform mat3  uObjRot[MAX_OBJECTS];
uniform vec4  uObjParams[MAX_OBJECTS];
uniform vec3  uObjColor[MAX_OBJECTS];
uniform float uObjBound[MAX_OBJECTS];

float sdObject(int i, vec3 p) {
    vec3 q = toLocal(p, uObjPos[i], uObjRot[i]);
    vec4 s = uObjParams[i];
    int type = uObjType[i];
    if (type == OBJ_SPHERE)     return sdSphere(q, s.x);
    if (type == OBJ_BOX)        return sdBox(q, s.xyz);
    if (type == OBJ_ROUND_BOX)  return sdRoundBox(q, s.xyz, s.w);
    if (type == OBJ_TORUS)      return sdTorus(q, s.xy);
    if (type == OBJ_CAPSULE)    return sdCapsule(q, vec3(0.0, -s.x, 0.0), vec3(0.0, s.x, 0.0), s.y);
    if (type == OBJ_CYLINDER)   return sdCylinder(q, s.y, s.x);
    if (type == OBJ_CONE)       return sdCappedCone(q, s.y, s.x, s.z);
    return sdOctahedron(q, s.x);
}

bool canSkip(int op, float d, float dBound, float k) {
    if (op == OP_UNION) return dBound - d >= k;
    if (op == OP_SUBTRACT) return dBound >= k - d;
    return false;
}

float sceneObjects(vec3 p, out vec3 color) {
    float d = 1e10;
    color = vec3(0.0);
    for (int i = 0; i < uObjectCount; i++) {
        float k = uObjK[i];
        int op = uObjOp[i];
        float dBound = length(p - uObjPos[i]) - uObjBound[i];
        if (canSkip(op, d, dBound, k)) continue;
        float di = sdObject(i, p);
        if (op == OP_UNION) {
            color = mix(color, uObjColor[i], sminBlend(d, di, k));
            d = smin(d, di, k);
        } else if (op == OP_SUBTRACT) {
            d = smax(d, -di, k);
        } else {
            d = smax(d, di, k);
        }
    }
    return d;
}

float scene(vec3 p, out vec3 color) {
    float d = sceneObjects(p, color);
    opUnion(d, color, sdPlane(p), vec3(0.45));
    return d;
}

float map(vec3 p) {
    vec3 unused;
    return scene(p, unused);
}

float mapObjects(vec3 p) {
    vec3 unused;
    return sceneObjects(p, unused);
}

vec3 sceneColor(vec3 p) {
    vec3 color;
    scene(p, color);
    return color;
}

float march(vec3 ro, vec3 rd, out int steps) {
    float t = 0;
    steps = 0;
    for (int i = 0; i < uMaxSteps; i++) {
        steps = i + 1;
        vec3 p = ro + t * rd;
        float d = map(p);
        if (d < uSurfEps) return t;
        t += d;
        if (t > uMaxDist) return -1.0;
    }
    return -1.0;
}

vec3 calcNormal(vec3 p) {
    float h = uSurfEps;
    vec2 e = vec2(h, 0.0);
    float base = map(p);
    return normalize(vec3(map(p + e.xyy) - base, map(p + e.yxy) - base, map(p + e.yyx) - base));
}

const float GAMMA = 2.2;
vec3 toLinear(vec3 c)  { return uGamma != 0 ? pow(c, vec3(GAMMA)) : c; }
vec3 toDisplay(vec3 c) { return uGamma != 0 ? pow(c, vec3(1.0 / GAMMA)) : c; }

float softShadow(vec3 ro, vec3 rd, float tmin, float tmax, float k) {
    float t = tmin;
    float sha = 1.0;
    for (int i = 0; i < 64; i++) {
        if (t > tmax) break;
        float h = mapObjects(ro + t * rd);
        sha = min(sha, k * h / t);
        if (sha < 0.001) break;
        t += h;
    }
    return clamp(sha, 0.0, 1.0);
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float weight = 1.0;
    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        occ += (h - map(p + h * n)) * weight;
        weight *= 0.95;
    }
    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}

vec3 shade(vec3 n, vec3 rd, vec3 albedo, float sha, float occ) {
    vec3 light = toLinear(uLightColor);
    float diffuse = max(dot(n, uLightDir), 0.0) * sha;

    float nDotL = max(dot(n, uLightDir), 0.0);
    float nDotH = max(dot(normalize(uLightDir - rd), n), 0.0);
    float specular = nDotL * sha * pow(nDotH, uShininess);

    return albedo * (uAmbient * occ + light * diffuse) + light * uSpecular * specular;
}

vec3 heatmap(float x) {
    x = clamp(x, 0.0, 1.0);
    return vec3(smoothstep(0.5, 1.0, x), 1.0 - abs(2.0 * x - 1.0), 1.0 - smoothstep(0.0, 0.5, x));
}

vec3 cameraRay(vec2 uv) {
    vec3 forward = normalize(uCamTarget - uCamPos);
    vec3 right   = normalize(cross(forward, vec3(0.0, 1.0, 0.0)));
    vec3 up      = cross(right, forward);
    float focal = 1.0 / tan(uFov * 0.5);
    return normalize(uv.x * right + uv.y * up + focal * forward);
}

void main() {
    float mn = min(uResolution.x, uResolution.y);
    vec2 uv = vec2((2.0 * gl_FragCoord.xy - uResolution.xy) / mn);

    vec3 ro = uCamPos;
    vec3 rd = cameraRay(uv);

    vec3 sky = toLinear(mix(vec3(0.85, 0.9, 1.0), vec3(0.35, 0.55, 0.9), clamp(rd.y * 2.0, 0.0, 1.0)));
    vec3 col = sky;

    int steps;
    float t = march(ro, rd, steps);
    vec3 n = vec3(0.0);
    float sha = 1.0, occ = 1.0;
    if (t > 0.0) {
        vec3 p = ro + t * rd;
        n = calcNormal(p);
        vec3 albedo = toLinear(sceneColor(p));
        if (uShadows != 0) sha = softShadow(p, uLightDir, 0.02, 20.0, uShadowK);
        if (uAO != 0) occ = calcAO(p, n);
        col = shade(n, rd, albedo, sha, occ);
        col = mix(col, sky, 1.0 - exp(-uFogDensity * t));
    }
    col = toDisplay(col);

    if (t > 0.0 && uDebugView == 1) col = n * 0.5 + 0.5;
    if (t > 0.0 && uDebugView == 3) col = vec3(sha);
    if (t > 0.0 && uDebugView == 4) col = vec3(occ);
    if (uDebugView == 2) col = heatmap(float(steps) / float(uMaxSteps));

    fragColor = vec4(col, 1.0);
}
