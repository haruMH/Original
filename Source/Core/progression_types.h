#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>

// =================================================================
// Material Types (Monster Hunter Progression)
// =================================================================
enum class MaterialType : int
{
    SCRAP_IRON = 0,       /* Scrap Iron */
    ENERGY_CELL,          /* Energy Cell */
    EXPLOSIVE_POWDER,     /* Explosive Powder */
    LIGHTNING_CORE,       /* Lightning Core */
    TITAN_ARMOR,          /* Titan Armor Plate */
    TITAN_CORE,           /* Titan Power Core */
    COUNT
};

// =================================================================
// Player Action / Ability Unlocks
// =================================================================
enum class AbilityType : int
{
    HEAVY_THROW = 0,          /* Heavy Throw */
    GIANT_SPIN,               /* Giant Spin */
    QUICK_GRAB,               /* Quick Grab */
    DASH_CANCEL,              /* Dash Cancel */
    QUICK_REVERSE,            /* Quick Turn */
    COUNTER_SHOCK,            /* Counter Shock */
    PARRY_EXTENSION,          /* Parry Time Extension */
    MATERIAL_VACUUM,          /* Material Vacuum */
    SURVIVAL_TOUGHNESS_1,     /* Toughness Lv1 */
    SURVIVAL_TOUGHNESS_2,     /* Toughness Lv2 */
    COUNT
};

// =================================================================
// Cost Definition
// =================================================================
struct MaterialCost
{
    MaterialType type;
    int          amount;
};

struct AbilityDef
{
    AbilityType               type;
    const wchar_t*            name;
    const wchar_t*            description;
    std::vector<MaterialCost> costs;
    AbilityType               prerequisite = AbilityType::COUNT;
};

// =================================================================
// Data Helper
// =================================================================
class ProgressionData
{
public:
    static const wchar_t* GetMaterialName(MaterialType type)
    {
        switch (type)
        {
        case MaterialType::SCRAP_IRON:       return L"くず鉄";
        case MaterialType::ENERGY_CELL:      return L"エネルギーセル";
        case MaterialType::EXPLOSIVE_POWDER: return L"不安定な火薬";
        case MaterialType::LIGHTNING_CORE:   return L"帯電プラグ";
        case MaterialType::TITAN_ARMOR:      return L"巨人の装甲片";
        case MaterialType::TITAN_CORE:       return L"巨人の動力核";
        default:                             return L"未知の素材";
        }
    }

    static const char* GetMaterialNameUtf8(MaterialType type)
    {
        switch (type)
        {
        case MaterialType::SCRAP_IRON:       return (const char*)u8"くず鉄";
        case MaterialType::ENERGY_CELL:      return (const char*)u8"エネルギーセル";
        case MaterialType::EXPLOSIVE_POWDER: return (const char*)u8"不安定な火薬";
        case MaterialType::LIGHTNING_CORE:   return (const char*)u8"帯電プラグ";
        case MaterialType::TITAN_ARMOR:      return (const char*)u8"巨人の装甲片";
        case MaterialType::TITAN_CORE:       return (const char*)u8"巨人の動力核";
        default:                             return (const char*)u8"未知の素材";
        }
    }

    static const wchar_t* GetMaterialDescription(MaterialType type)
    {
        switch (type)
        {
        case MaterialType::SCRAP_IRON:       return L"敵の残骸から回収された鉄板。加工しやすい。";
        case MaterialType::ENERGY_CELL:      return L"遠距離兵器に用いられる小型のバッテリー。";
        case MaterialType::EXPLOSIVE_POWDER: return L"高熱を秘めた火薬。衝撃で起爆する性質を持つ。";
        case MaterialType::LIGHTNING_CORE:   return L"強烈な電圧を蓄えたプラグ。青く放電している。";
        case MaterialType::TITAN_ARMOR:      return L"巨躯の装甲から削り取られた強固な合金プレート。";
        case MaterialType::TITAN_CORE:       return L"巨大ボスの動力源。絶大なエネルギーを放つ。";
        default:                             return L"";
        }
    }

    static DirectX::XMFLOAT3 GetMaterialColor(MaterialType type)
    {
        switch (type)
        {
        case MaterialType::SCRAP_IRON:       return DirectX::XMFLOAT3(0.8f, 0.8f, 0.9f);
        case MaterialType::ENERGY_CELL:      return DirectX::XMFLOAT3(0.2f, 0.9f, 0.3f);
        case MaterialType::EXPLOSIVE_POWDER: return DirectX::XMFLOAT3(1.0f, 0.3f, 0.1f);
        case MaterialType::LIGHTNING_CORE:   return DirectX::XMFLOAT3(0.2f, 0.7f, 1.0f);
        case MaterialType::TITAN_ARMOR:      return DirectX::XMFLOAT3(0.9f, 0.7f, 0.2f);
        case MaterialType::TITAN_CORE:       return DirectX::XMFLOAT3(1.0f, 0.2f, 0.8f);
        default:                             return DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        }
    }

    static const AbilityDef& GetAbilityDef(AbilityType type)
    {
        static const AbilityDef s_Defs[static_cast<int>(AbilityType::COUNT)] = {
            {
                AbilityType::HEAVY_THROW,
                L"ヘビースロー",
                L"敵投擲時の初速と飛距離が25%向上する。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 3 } }
            },
            {
                AbilityType::GIANT_SPIN,
                L"ジャイアントスピン",
                L"スピン攻撃時の旋回速度と巻き込み範囲が拡大する。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 4 }, MaterialCost{ MaterialType::ENERGY_CELL, 1 } }
            },
            {
                AbilityType::QUICK_GRAB,
                L"クイックグラブ",
                L"敵を掴む判定距離が25%延長され、素早く捕らえられる。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 3 }, MaterialCost{ MaterialType::ENERGY_CELL, 1 } }
            },
            {
                AbilityType::DASH_CANCEL,
                L"ダッシュキャンセル",
                L"攻撃後や敵投擲直後の硬直を、ダッシュで即座にキャンセル可能にする。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 5 }, MaterialCost{ MaterialType::EXPLOSIVE_POWDER, 2 } }
            },
            {
                AbilityType::QUICK_REVERSE,
                L"クイックターン",
                L"方向転換時の旋回レスポンスが向上し、素早く振り返れる。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 2 } }
            },
            {
                AbilityType::COUNTER_SHOCK,
                L"カウンターショック",
                L"パリィ成功時、周囲に微小な衝撃波を放ち近接敵を吹き飛ばす。",
                { MaterialCost{ MaterialType::LIGHTNING_CORE, 2 }, MaterialCost{ MaterialType::TITAN_ARMOR, 1 } }
            },
            {
                AbilityType::PARRY_EXTENSION,
                L"パリィタイム延長",
                L"パリィ時のスローモーション演出時間が1.5倍に延長される。",
                { MaterialCost{ MaterialType::ENERGY_CELL, 3 }, MaterialCost{ MaterialType::TITAN_ARMOR, 1 } }
            },
            {
                AbilityType::MATERIAL_VACUUM,
                L"素材バキューム",
                L"素材アイテムの自動吸引範囲が2.5倍に拡大し、回収がスムーズになる。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 4 }, MaterialCost{ MaterialType::ENERGY_CELL, 2 } }
            },
            {
                AbilityType::SURVIVAL_TOUGHNESS_1,
                L"タフネス Lv1",
                L"最大HPが +1 される（初期3 → 最大4）。",
                { MaterialCost{ MaterialType::SCRAP_IRON, 5 }, MaterialCost{ MaterialType::TITAN_ARMOR, 2 } }
            },
            {
                AbilityType::SURVIVAL_TOUGHNESS_2,
                L"タフネス Lv2",
                L"最大HPがさらに +1 される（最大5）。",
                { MaterialCost{ MaterialType::TITAN_ARMOR, 3 }, MaterialCost{ MaterialType::TITAN_CORE, 1 } },
                AbilityType::SURVIVAL_TOUGHNESS_1
            }
        };

        int idx = static_cast<int>(type);
        if (idx >= 0 && idx < static_cast<int>(AbilityType::COUNT))
        {
            return s_Defs[idx];
        }
        return s_Defs[0];
    }
};
