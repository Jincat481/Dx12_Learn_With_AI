// =============================================================
// OceanSim.hlsl - 바다 파도 텍스처 계산 (S85, S86)
//  렌더 타깃 두 장(MRT)에 한 번에 쓴다.
//   SV_Target0 : 변위 (x, y, z) + 거품(w)
//   SV_Target1 : 기울기 ∂y/∂x, ∂y/∂z + 수평 변위의 늘어남 ∂x/∂x, ∂z/∂z
//
//  바다 수면 = 방향 · 파장 · 진폭이 다른 수많은 파도의 합이다.
//  CPU 가 바람에 맞는 스펙트럼에서 파도를 골라 상수 버퍼로 넘기고, 여기서 텍셀마다 더한다.
//
//  깊은 바다의 분산 관계 : ω = √(g · |k|)
//   파장이 긴 파도일수록 빨리 달린다. 큰 너울이 잔물결을 앞질러 가는 모습이 여기서 나온다.
//
//  게르스트너(뾰족한 파도) :
//   높이만 흔들면 사인파라 마루와 골이 똑같이 둥글다. 수면의 점을 파도 진행 방향으로
//   앞뒤로도 움직이면 점들이 마루로 몰려 마루는 뾰족하고 골은 넓어진다. 실제 바다 파도 모양이다.
//      y  = Σ A cos θ
//      xz = -λ Σ k̂ A sin θ            (θ = k·p - ωt + φ,  λ = 뾰족함)
//
//  야코비안 (S86) :
//   수평으로 움직인 뒤 면적이 얼마나 줄었는가. J = (1+∂x/∂x)(1+∂z/∂z) - (∂x/∂z)²
//   마루에서 물이 몰리면 J 가 작아지고, 0 밑으로 가면 면이 접힌다(부서지는 파도).
//   J 가 작은 곳에 흰 거품을 만들고, 지난 프레임 거품은 서서히 사라지게 둔다.
// =============================================================

cbuffer OceanSimConstants : register(b0)
{
    float4 gTile;        // x : 타일 한 변(m)   y : 시간(초)   z : 뾰족함 λ   w : 파도 개수
    float4 gFoam;        // x : 이 야코비안보다 작으면 거품   y : 생성량   z : 이번 프레임 유지율   w : 텍스처 크기
    float4 gWaves[64];   // xy : 파수 벡터 k (rad/m)   z : 진폭(m)   w : 위상
};

Texture2D<float4> gPrevious : register(t0);   // 지난 프레임 변위 텍스처 (거품을 이어받는다)

struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

FullscreenVertex VSMain(uint id : SV_VertexID)
{
    FullscreenVertex output;
    float2 corner = float2((id << 1) & 2, id & 2);
    output.position = float4(corner * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

struct OceanTargets
{
    float4 displacement : SV_Target0;
    float4 slope        : SV_Target1;
};

OceanTargets PSMain(FullscreenVertex input)
{
    const float gravity = 9.81f;

    // 텍셀 → 타일 안의 위치(m). 파수는 타일에 딱 맞게 골랐으므로 타일을 이어 붙여도 이음매가 없다.
    int2 texel = int2(input.position.xy);
    float2 p = (float2(texel) + 0.5f) / gFoam.w * gTile.x;

    const float time = gTile.y;
    const float lambda = gTile.z;

    float3 displacement = float3(0.0f, 0.0f, 0.0f);
    float dydx = 0.0f, dydz = 0.0f;
    float dxdx = 0.0f, dzdz = 0.0f, dxdz = 0.0f;

    uint count = (uint)gTile.w;

    [loop]
    for (uint i = 0; i < count; ++i)
    {
        float2 k = gWaves[i].xy;
        float amplitude = gWaves[i].z;
        float kLength = max(length(k), 1e-4f);
        float2 direction = k / kLength;

        float omega = sqrt(gravity * kLength);
        float theta = dot(k, p) - omega * time + gWaves[i].w;
        float s = sin(theta);
        float c = cos(theta);

        displacement.y  += amplitude * c;
        displacement.xz -= lambda * direction * amplitude * s;

        // 미분은 식을 그대로 미분해 적는다 (중앙 차분보다 정확하고 텍셀 크기와 무관하다)
        dydx -= amplitude * k.x * s;
        dydz -= amplitude * k.y * s;

        float chop = lambda * amplitude * c / kLength;
        dxdx -= chop * k.x * k.x;
        dzdz -= chop * k.y * k.y;
        dxdz -= chop * k.x * k.y;
    }

    // ---- 거품 (S86) ----
    float jacobian = (1.0f + dxdx) * (1.0f + dzdz) - dxdz * dxdz;
    float created = saturate((gFoam.x - jacobian) * gFoam.y);
    float previous = gPrevious.Load(int3(texel, 0)).w;
    float foam = max(previous * gFoam.z, created);

    OceanTargets output;
    output.displacement = float4(displacement, foam);
    output.slope = float4(dydx, dydz, dxdx, dzdz);
    return output;
}
