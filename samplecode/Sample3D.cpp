/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRRect.h"
#include "include/private/SkM44.h"
#include "include/utils/SkRandom.h"
#include "samplecode/Sample.h"
#include "tools/Resources.h"

struct VSphere {
    SkVec2   fCenter;
    SkScalar fRadius;

    VSphere(SkVec2 center, SkScalar radius) : fCenter(center), fRadius(radius) {}

    bool contains(SkVec2 v) const {
        return (v - fCenter).length() <= fRadius;
    }

    SkVec2 pinLoc(SkVec2 p) const {
        auto v = p - fCenter;
        if (v.length() > fRadius) {
            v *= (fRadius / v.length());
        }
        return fCenter + v;
    }

    SkV3 computeUnitV3(SkVec2 v) const {
        v = (v - fCenter) * (1 / fRadius);
        SkScalar len2 = v.lengthSquared();
        if (len2 > 1) {
            v = v.normalize();
            len2 = 1;
        }
        SkScalar z = SkScalarSqrt(1 - len2);
        return {v.x, v.y, z};
    }

    struct RotateInfo {
        SkV3    fAxis;
        SkScalar fAngle;
    };

    RotateInfo computeRotationInfo(SkVec2 a, SkVec2 b) const {
        SkV3 u = this->computeUnitV3(a);
        SkV3 v = this->computeUnitV3(b);
        SkV3 axis = u.cross(v);
        SkScalar length = axis.length();

        if (!SkScalarNearlyZero(length)) {
            return {axis * (1.0f / length), acos(u.dot(v))};
        }
        return {{0, 0, 0}, 0};
    }

    SkM44 computeRotation(SkVec2 a, SkVec2 b) const {
        auto [axis, angle] = this->computeRotationInfo(a, b);
        return SkM44::Rotate(axis, angle);
    }
};

static SkM44 inv(const SkM44& m) {
    SkM44 inverse;
    SkAssertResult(m.invert(&inverse));
    return inverse;
}

class Sample3DView : public Sample {
protected:
    float   fNear = 0.05f;
    float   fFar = 4;
    float   fAngle = SK_ScalarPI / 12;

    SkV3    fEye { 0, 0, 1.0f/tan(fAngle/2) - 1 };
    SkV3    fCOA { 0, 0, 0 };
    SkV3    fUp  { 0, 1, 0 };

public:
    void saveCamera(SkCanvas* canvas, const SkRect& area, SkScalar zscale) {
        SkM44 camera = Sk3LookAt(fEye, fCOA, fUp),
              perspective = Sk3Perspective(fNear, fFar, fAngle),
              viewport = SkM44::Translate(area.centerX(), area.centerY(), 0) *
                         SkM44::Scale(area.width()*0.5f, area.height()*0.5f, zscale);

        // want "world" to be in our big coordinates (e.g. area), so apply this inverse
        // as part of our "camera".
        canvas->experimental_saveCamera(viewport * perspective, camera * inv(viewport));
    }
};

struct Face {
    SkScalar fRx, fRy;
    SkColor  fColor;

    static SkM44 T(SkScalar x, SkScalar y, SkScalar z) {
        return SkM44::Translate(x, y, z);
    }

    static SkM44 R(SkV3 axis, SkScalar rad) {
        return SkM44::Rotate(axis, rad);
    }

    SkM44 asM44(SkScalar scale) const {
        return R({0,1,0}, fRy) * R({1,0,0}, fRx) * T(0, 0, scale);
    }
};

static bool front(const SkM44& m) {
    SkM44 m2(SkM44::kUninitialized_Constructor);
    if (!m.invert(&m2)) {
        m2.setIdentity();
    }
    /*
     *  Classically we want to dot the transpose(inverse(ctm)) with our surface normal.
     *  In this case, the normal is known to be {0, 0, 1}, so we only actually need to look
     *  at the z-scale of the inverse (the transpose doesn't change the main diagonal, so
     *  no need to actually transpose).
     */
    return m2.rc(2,2) > 0;
}

const Face faces[] = {
    {             0,             0,  SK_ColorRED }, // front
    {             0,   SK_ScalarPI,  SK_ColorGREEN }, // back

    { SK_ScalarPI/2,             0,  SK_ColorBLUE }, // top
    {-SK_ScalarPI/2,             0,  SK_ColorCYAN }, // bottom

    {             0, SK_ScalarPI/2,  SK_ColorMAGENTA }, // left
    {             0,-SK_ScalarPI/2,  SK_ColorYELLOW }, // right
};

#include "include/effects/SkRuntimeEffect.h"

struct LightOnSphere {
    SkVec2   fLoc;
    SkScalar fDistance;
    SkScalar fRadius;

    SkV3 computeWorldPos(const VSphere& s) const {
        return s.computeUnitV3(fLoc) * fDistance;
    }

    void draw(SkCanvas* canvas) const {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor(SK_ColorWHITE);
        canvas->drawCircle(fLoc.x, fLoc.y, fRadius + 2, paint);
        paint.setColor(SK_ColorBLACK);
        canvas->drawCircle(fLoc.x, fLoc.y, fRadius, paint);
    }
};

#include "include/core/SkTime.h"

class RotateAnimator {
    SkV3        fAxis = {0, 0, 0};
    SkScalar    fAngle = 0,
                fPrevAngle = 1234567;
    double      fNow = 0,
                fPrevNow = 0;

    SkScalar    fAngleSpeed = 0,
                fAngleSign = 1;

    static constexpr double kSlowDown = 4;
    static constexpr SkScalar kMaxSpeed = 16;

public:
    void update(SkV3 axis, SkScalar angle) {
        if (angle != fPrevAngle) {
            fPrevAngle = fAngle;
            fAngle = angle;

            fPrevNow = fNow;
            fNow = SkTime::GetSecs();

            fAxis = axis;
        }
    }

    SkM44 rotation() {
        if (fAngleSpeed > 0) {
            double now = SkTime::GetSecs();
            double dtime = now - fPrevNow;
            fPrevNow = now;
            double delta = fAngleSign * fAngleSpeed * dtime;
            fAngle += delta;
            fAngleSpeed -= kSlowDown * dtime;
            if (fAngleSpeed < 0) {
                fAngleSpeed = 0;
            }
        }
        return SkM44::Rotate(fAxis, fAngle);

    }

    void start() {
        if (fPrevNow != fNow) {
            fAngleSpeed = (fAngle - fPrevAngle) / (fNow - fPrevNow);
            fAngleSign = fAngleSpeed < 0 ? -1 : 1;
            fAngleSpeed = std::min(kMaxSpeed, std::abs(fAngleSpeed));
        } else {
            fAngleSpeed = 0;
        }
        fPrevNow = SkTime::GetSecs();
        fAngle = 0;
    }

    void reset() {
        fAngleSpeed = 0;
        fAngle = 0;
        fPrevAngle = 1234567;
    }
};

class SampleCubeBase : public Sample3DView {
    enum {
        DX = 400,
        DY = 300
    };

    SkM44 fWorldToClick,
          fClickToWorld;

    SkM44 fRotation;        // part of model

    RotateAnimator fRotateAnimator;

protected:
    enum Flags {
        kCanRunOnCPU    = 1 << 0,
        kShowLightDome  = 1 << 1,
    };

    LightOnSphere fLight = {{200 + DX, 200 + DY}, 800, 12};

    VSphere fSphere;
    Flags   fFlags;

public:
    SampleCubeBase(Flags flags)
        : fSphere({200 + DX, 200 + DY}, 400)
        , fFlags(flags)
    {}

    bool onChar(SkUnichar uni) override {
        switch (uni) {
            case 'Z': fLight.fDistance += 10; return true;
            case 'z': fLight.fDistance -= 10; return true;
        }
        return this->Sample3DView::onChar(uni);
    }

    virtual void drawContent(SkCanvas* canvas, SkColor, int index, bool drawFront) = 0;

    void setClickToWorld(SkCanvas* canvas, const SkM44& clickM) {
        auto l2d = canvas->experimental_getLocalToDevice();
        fWorldToClick = inv(clickM) * l2d;
        fClickToWorld = inv(fWorldToClick);
    }

    void onDrawContent(SkCanvas* canvas) override {
        if (!canvas->getGrContext() && !(fFlags & kCanRunOnCPU)) {
            return;
        }
        SkM44 clickM = canvas->experimental_getLocalToDevice();

        canvas->save();
        canvas->translate(DX, DY);

        this->saveCamera(canvas, {0, 0, 400, 400}, 200);

        this->setClickToWorld(canvas, clickM);

        for (bool drawFront : {false, true}) {
            int index = 0;
            for (auto f : faces) {
                SkAutoCanvasRestore acr(canvas, true);

                SkM44 trans = SkM44::Translate(200, 200, 0);   // center of the rotation
                SkM44 m = fRotateAnimator.rotation() * fRotation * f.asM44(200);

                canvas->experimental_concat44(trans * m * inv(trans));
                this->drawContent(canvas, f.fColor, index++, drawFront);
            }
        }

        canvas->restore();  // camera
        canvas->restore();  // center the content in the window

        if (fFlags & kShowLightDome){
            fLight.draw(canvas);

            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setColor(0x40FF0000);
            canvas->drawCircle(fSphere.fCenter.x, fSphere.fCenter.y, fSphere.fRadius, paint);
            canvas->drawLine(fSphere.fCenter.x, fSphere.fCenter.y - fSphere.fRadius,
                             fSphere.fCenter.x, fSphere.fCenter.y + fSphere.fRadius, paint);
            canvas->drawLine(fSphere.fCenter.x - fSphere.fRadius, fSphere.fCenter.y,
                             fSphere.fCenter.x + fSphere.fRadius, fSphere.fCenter.y, paint);
        }
    }

    Click* onFindClickHandler(SkScalar x, SkScalar y, skui::ModifierKey modi) override {
        SkVec2 p = fLight.fLoc - SkVec2{x, y};
        if (p.length() <= fLight.fRadius) {
            Click* c = new Click();
            c->fMeta.setS32("type", 0);
            return c;
        }
        if (fSphere.contains({x, y})) {
            Click* c = new Click();
            c->fMeta.setS32("type", 1);

            fRotation = fRotateAnimator.rotation() * fRotation;
            fRotateAnimator.reset();
            return c;
        }
        return nullptr;
    }
    bool onClick(Click* click) override {
#if 0
        auto L = fWorldToClick * fLight.fPos;
        SkPoint c = project(fClickToWorld, {click->fCurr.fX, click->fCurr.fY, L.z/L.w, 1});
        fLight.update(c.fX, c.fY);
#endif
        if (click->fMeta.hasS32("type", 0)) {
            fLight.fLoc = fSphere.pinLoc({click->fCurr.fX, click->fCurr.fY});
            return true;
        }
        if (click->fMeta.hasS32("type", 1)) {
            if (click->fState == skui::InputState::kUp) {
                fRotation = fRotateAnimator.rotation() * fRotation;
                fRotateAnimator.start();
            } else {
                auto [axis, angle] = fSphere.computeRotationInfo(
                                                {click->fOrig.fX, click->fOrig.fY},
                                                {click->fCurr.fX, click->fCurr.fY});
                fRotateAnimator.update(axis, angle);
            }
            return true;
        }
        return true;
    }

    bool onAnimate(double nanos) override {
        // handle fling
        return this->INHERITED::onAnimate(nanos);
    }

private:
    typedef Sample3DView INHERITED;
};

class SampleBump3D : public SampleCubeBase {
    sk_sp<SkShader>        fBmpShader, fImgShader;
    sk_sp<SkRuntimeEffect> fEffect;
    SkRRect                fRR;

public:
    SampleBump3D() : SampleCubeBase(kShowLightDome) {}

    SkString name() override { return SkString("bump3d"); }

    void onOnceBeforeDraw() override {
        fRR = SkRRect::MakeRectXY({20, 20, 380, 380}, 50, 50);
        auto img = GetResourceAsImage("images/brickwork-texture.jpg");
        fImgShader = img->makeShader(SkMatrix::MakeScale(2, 2));
        img = GetResourceAsImage("images/brickwork_normal-map.jpg");
        fBmpShader = img->makeShader(SkMatrix::MakeScale(2, 2));

        const char code[] = R"(
            in fragmentProcessor color_map;
            in fragmentProcessor normal_map;

            uniform float4x4 localToWorld;
            uniform float4x4 localToWorldAdjInv;
            uniform float3   lightPos;

            float3 convert_normal_sample(half4 c) {
                float3 n = 2 * c.rgb - 1;
                n.y = -n.y;
                return n;
            }

            void main(float2 p, inout half4 color) {
                float3 norm = convert_normal_sample(sample(normal_map, p));
                float3 plane_norm = normalize(localToWorld * float4(norm, 0)).xyz;

                float3 plane_pos = (localToWorld * float4(p, 0, 1)).xyz;
                float3 light_dir = normalize(lightPos - plane_pos);

                float ambient = 0.2;
                float dp = dot(plane_norm, light_dir);
                float scale = min(ambient + max(dp, 0), 1);

                color = sample(color_map, p) * half4(float4(scale, scale, scale, 1));
            }
        )";
        auto [effect, error] = SkRuntimeEffect::Make(SkString(code));
        if (!effect) {
            SkDebugf("runtime error %s\n", error.c_str());
        }
        fEffect = effect;
    }

    void drawContent(SkCanvas* canvas, SkColor color, int index, bool drawFront) override {
        if (!drawFront || !front(canvas->experimental_getLocalToDevice())) {
            return;
        }

        auto adj_inv = [](const SkM44& m) {
            SkM44 inv;
            SkAssertResult(m.invert(&inv));
            return inv.transpose();
        };

        struct Uniforms {
            SkM44  fLocalToWorld;
            SkM44  fLocalToWorldAdjInv;
            SkV3   fLightPos;
        } uni;
        uni.fLocalToWorld = canvas->experimental_getLocalToWorld();
        uni.fLocalToWorldAdjInv = adj_inv(uni.fLocalToWorld);
        uni.fLightPos = fLight.computeWorldPos(fSphere);

        sk_sp<SkData> data = SkData::MakeWithCopy(&uni, sizeof(uni));
        sk_sp<SkShader> children[] = { fImgShader, fBmpShader };

        SkPaint paint;
        paint.setColor(color);
        paint.setShader(fEffect->makeShader(data, children, 2, nullptr, true));

        canvas->drawRRect(fRR, paint);
    }
};
DEF_SAMPLE( return new SampleBump3D; )

#include "modules/skottie/include/Skottie.h"

class SampleSkottieCube : public SampleCubeBase {
    sk_sp<skottie::Animation> fAnim[6];

public:
    SampleSkottieCube() : SampleCubeBase(kCanRunOnCPU) {}

    SkString name() override { return SkString("skottie3d"); }

    void onOnceBeforeDraw() override {
        const char* files[] = {
            "skottie/skottie-chained-mattes.json",
            "skottie/skottie-gradient-ramp.json",
            "skottie/skottie_sample_2.json",
            "skottie/skottie-3d-3planes.json",
            "skottie/skottie-text-animator-4.json",
            "skottie/skottie-motiontile-effect-phase.json",

        };
        for (unsigned i = 0; i < SK_ARRAY_COUNT(files); ++i) {
            if (auto stream = GetResourceAsStream(files[i])) {
                fAnim[i] = skottie::Animation::Make(stream.get());
            }
        }
    }

    void drawContent(SkCanvas* canvas, SkColor color, int index, bool drawFront) override {
        if (!drawFront || !front(canvas->experimental_getLocalToDevice())) {
            return;
        }

        SkPaint paint;
        paint.setColor(color);
        SkRect r = {0, 0, 400, 400};
        canvas->drawRect(r, paint);
        fAnim[index]->render(canvas, &r);
    }

    bool onAnimate(double nanos) override {
        for (auto& anim : fAnim) {
            SkScalar dur = anim->duration();
            SkScalar t = fmod(1e-9 * nanos, dur) / dur;
            anim->seek(t);
        }
        return true;
    }
};
DEF_SAMPLE( return new SampleSkottieCube; )
