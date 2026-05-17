#pragma once
#include <osg/ref_ptr>
#include <osg/Group>
#include <osg/Program>
#include <osg/Uniform>
#include <osg/StateSet>
#include <vector>
#include <array>

class PreProcessManager {
public:
    enum Effect {
        // 渲染风格 (0~9)
        PhongEnhanced = 0,
        CelShading,
        HalfLambert,
        AnisotropicSpecular,
        SubsurfaceScatter,
        RimLight,
        MatCap,
        Metalness,
        OutlineStroke,
        WireframeOverlay,
        // 材质特效 (10~17)
        XRay,
        Hologram,
        EnergyShield,
        Dissolve,
        Frozen,
        Lava,
        Camouflage,
        CrystalGlass,
        // 环境氛围 (18~21)
        DistanceFog,
        HeightFog,
        AtmosphericScatter,
        GroundFog,
        // 动画变形 (22~29)
        VertexWave,
        FlagWave,
        WaterRipple,
        PulseBreathe,
        ScanLine,
        ElectricArc,
        BurnEffect,
        GlitchEffect,
        // 可视化诊断 (30~35)
        NormalVis,
        DepthVis,
        VertexColorVis,
        UVVis,
        FaceOrientation,
        HeightColor,
        // 动画变形扩展 (36)
        Explode,
        EffectCount
    };

    PreProcessManager();
    ~PreProcessManager();

    void attach(osg::Group* root);
    void detach();
    void updateTime(float t);

    void setEffectEnabled(Effect e, bool on);
    void setEffectParam0(Effect e, float v);
    void setEffectParam1(Effect e, float v);
    void setEffectColor(Effect e, float r, float g, float b);

    bool  isEffectEnabled(Effect e) const { return m_params[static_cast<int>(e)].enabled; }
    float effectParam0(Effect e)    const { return m_params[static_cast<int>(e)].param0; }
    float effectParam1(Effect e)    const { return m_params[static_cast<int>(e)].param1; }

    bool isAttached() const { return m_attached; }
    bool hasEnabledEffects() const;

private:
    void buildProgram();
    void syncUniforms();

    struct EffectParams {
        bool  enabled = false;
        float param0  = 0.5f;
        float param1  = 0.5f;
        float colorR  = 1.0f;
        float colorG  = 1.0f;
        float colorB  = 1.0f;
    };

    struct Binding {
        osg::ref_ptr<osg::StateSet> stateSet;
        osg::ref_ptr<osg::Uniform> effectMask0;
        osg::ref_ptr<osg::Uniform> effectMask1;
        osg::ref_ptr<osg::Uniform> param0;
        osg::ref_ptr<osg::Uniform> param1;
        osg::ref_ptr<osg::Uniform> color;
        osg::ref_ptr<osg::Uniform> time;
        osg::ref_ptr<osg::Uniform> useBaseTex;
        osg::ref_ptr<osg::Uniform> baseTex;
        osg::ref_ptr<osg::Uniform> sceneScale;
        osg::ref_ptr<osg::Uniform> partCenter;
        osg::ref_ptr<osg::Uniform> sceneCenter;
    };

    osg::ref_ptr<osg::Program> m_program;
    std::vector<Binding>       m_bindings;

    std::array<EffectParams, EffectCount> m_params;
    float       m_time     = 0.0f;
    float       m_sceneScale = 1.0f;
    bool        m_attached = false;
    osg::Group* m_root     = nullptr;
};
