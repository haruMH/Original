#pragma once
#include <DirectXMath.h>

#ifdef Low
#undef Low
#endif
#ifdef Medium
#undef Medium
#endif
#ifdef High
#undef High
#endif
#ifdef Ultra
#undef Ultra
#endif

// =================================================================
// Graphics Settings (Presets & Detailed Customization)
// =================================================================

// Presets
enum class GraphicsPreset {
    Preset_Ultra = 0,
    Preset_High,
    Preset_Medium,
    Preset_Low,
    Preset_Custom
};

// Shadow Quality
enum class ShadowQuality {
    Shadow_Ultra = 0,
    Shadow_High,
    Shadow_Medium,
    Shadow_Low
};

// Mesh Outline
enum class OutlineQuality {
    Outline_All = 0,
    Outline_Characters,
    Outline_Off
};

// Anisotropic Filtering
enum class AnisoQuality {
    Aniso16x = 0,
    Aniso8x,
    Aniso4x,
    Aniso2x
};

// Post Processing Quality
enum class PostProcessQuality {
    Post_High = 0,
    Post_Medium,
    Post_Low
};

// Field Mesh Quality
enum class FieldMeshQuality {
    Mesh_High = 0,
    Mesh_Medium,
    Mesh_Low
};

// Graphics settings configuration struct
struct GraphicsSettings {
    GraphicsPreset      preset          = GraphicsPreset::Preset_Ultra;
    float               renderScale     = 1.0f;    // 0.5f - 1.0f (Render scale)
    float               casSharpness    = 0.60f;   // 0.0f - 1.0f (CAS Sharpness)
    
    // Geometry & Mesh
    OutlineQuality      outline         = OutlineQuality::Outline_All;
    FieldMeshQuality    fieldMesh       = FieldMeshQuality::Mesh_High;
    bool                subMeshDetail   = true;    // Character decorative parts
    bool                skyboxHigh      = true;    // High detail skydome
    
    // Shadow
    ShadowQuality       shadowQuality   = ShadowQuality::Shadow_Ultra;
    bool                blobShadow      = true;    // Ground drop shadow
    
    // Texture & Reflection
    AnisoQuality        anisotropy      = AnisoQuality::Aniso16x;
    bool                refractionCopy  = true;    // Realtime refraction copy
    
    // Post Processing & Environment
    PostProcessQuality  postProcess     = PostProcessQuality::Post_High;
    bool                radialBlur      = true;    // Action radial blur
    bool                heightFog       = true;    // Atmospheric fog
    int                 maxDamagePopups = 0;       // Max damage popups (0: unlimited)

    // Preset application
    void ApplyPreset(GraphicsPreset p) {
        preset = p;
        switch (p) {
        case GraphicsPreset::Preset_Ultra:
            renderScale     = 1.0f;
            casSharpness    = 0.60f;
            outline         = OutlineQuality::Outline_All;
            fieldMesh       = FieldMeshQuality::Mesh_High;
            subMeshDetail   = true;
            skyboxHigh      = true;
            shadowQuality   = ShadowQuality::Shadow_Ultra;
            blobShadow      = true;
            anisotropy      = AnisoQuality::Aniso16x;
            refractionCopy  = true;
            postProcess     = PostProcessQuality::Post_High;
            radialBlur      = true;
            heightFog       = true;
            maxDamagePopups = 0;
            break;
        case GraphicsPreset::Preset_High:
            renderScale     = 1.0f;
            casSharpness    = 0.65f;
            outline         = OutlineQuality::Outline_All;
            fieldMesh       = FieldMeshQuality::Mesh_High;
            subMeshDetail   = true;
            skyboxHigh      = true;
            shadowQuality   = ShadowQuality::Shadow_High;
            blobShadow      = true;
            anisotropy      = AnisoQuality::Aniso8x;
            refractionCopy  = true;
            postProcess     = PostProcessQuality::Post_High;
            radialBlur      = true;
            heightFog       = true;
            maxDamagePopups = 0;
            break;
        case GraphicsPreset::Preset_Medium:
            renderScale     = 0.90f;
            casSharpness    = 0.75f;
            outline         = OutlineQuality::Outline_Characters;
            fieldMesh       = FieldMeshQuality::Mesh_Medium;
            subMeshDetail   = true;    // キャラクター装飾・目は常時有効
            skyboxHigh      = true;
            shadowQuality   = ShadowQuality::Shadow_Medium;
            blobShadow      = true;
            anisotropy      = AnisoQuality::Aniso4x;
            refractionCopy  = false;
            postProcess     = PostProcessQuality::Post_Medium;
            radialBlur      = true;
            heightFog       = false;
            maxDamagePopups = 5;
            break;
        case GraphicsPreset::Preset_Low:
            renderScale     = 0.75f;
            casSharpness    = 0.85f;
            outline         = OutlineQuality::Outline_Off;
            fieldMesh       = FieldMeshQuality::Mesh_Low;
            subMeshDetail   = true;    // キャラクター装飾・目は常時有効
            skyboxHigh      = false;
            shadowQuality   = ShadowQuality::Shadow_Low;
            blobShadow      = false;
            anisotropy      = AnisoQuality::Aniso2x;
            refractionCopy  = false;
            postProcess     = PostProcessQuality::Post_Low;
            radialBlur      = false;
            heightFog       = false;
            maxDamagePopups = -1;
            break;
        case GraphicsPreset::Preset_Custom:
            break;
        }
    }
};

// Global graphics configuration
class GraphicsConfig {
private:
    static GraphicsSettings s_Settings;
    static bool             s_Dirty;

public:
    static GraphicsSettings& Get() { return s_Settings; }
    static const GraphicsSettings& GetConst() { return s_Settings; }

    static void SetPreset(GraphicsPreset p) {
        s_Settings.ApplyPreset(p);
        s_Dirty = true;
    }

    static void MarkCustom() {
        s_Settings.preset = GraphicsPreset::Preset_Custom;
        s_Dirty = true;
    }

    static bool ConsumeDirty() {
        bool d = s_Dirty;
        s_Dirty = false;
        return d;
    }
};
