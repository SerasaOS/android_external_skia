/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**************************************************************************************************
 *** This file was autogenerated from GrRRectBlurEffect.fp; do not modify.
 **************************************************************************************************/
#include "GrRRectBlurEffect.h"

std::unique_ptr<GrFragmentProcessor> GrRRectBlurEffect::Make(
        std::unique_ptr<GrFragmentProcessor> inputFP,
        GrRecordingContext* context,
        float sigma,
        float xformedSigma,
        const SkRRect& srcRRect,
        const SkRRect& devRRect) {
    SkASSERT(!SkRRectPriv::IsCircle(devRRect) &&
             !devRRect.isRect());  // Should've been caught up-stream

    // TODO: loosen this up
    if (!SkRRectPriv::IsSimpleCircular(devRRect)) {
        return nullptr;
    }

    // Make sure we can successfully ninepatch this rrect -- the blur sigma has to be
    // sufficiently small relative to both the size of the corner radius and the
    // width (and height) of the rrect.
    SkRRect rrectToDraw;
    SkISize dimensions;
    SkScalar ignored[kSkBlurRRectMaxDivisions];
    int ignoredSize;
    uint32_t ignored32;

    bool ninePatchable = SkComputeBlurredRRectParams(
            srcRRect, devRRect, SkRect::MakeEmpty(), sigma, xformedSigma, &rrectToDraw, &dimensions,
            ignored, ignored, ignored, ignored, &ignoredSize, &ignoredSize, &ignored32);
    if (!ninePatchable) {
        return nullptr;
    }

    std::unique_ptr<GrFragmentProcessor> maskFP =
            find_or_create_rrect_blur_mask_fp(context, rrectToDraw, dimensions, xformedSigma);
    if (!maskFP) {
        return nullptr;
    }

    return std::unique_ptr<GrFragmentProcessor>(
            new GrRRectBlurEffect(std::move(inputFP), xformedSigma, devRRect.getBounds(),
                                  SkRRectPriv::GetSimpleRadii(devRRect).fX, std::move(maskFP)));
}
#include "src/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLRRectBlurEffect : public GrGLSLFragmentProcessor {
public:
    GrGLSLRRectBlurEffect() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrRRectBlurEffect& _outer = args.fFp.cast<GrRRectBlurEffect>();
        (void)_outer;
        auto sigma = _outer.sigma;
        (void)sigma;
        auto rect = _outer.rect;
        (void)rect;
        auto cornerRadius = _outer.cornerRadius;
        (void)cornerRadius;
        cornerRadiusVar = args.fUniformHandler->addUniform(&_outer, kFragment_GrShaderFlag,
                                                           kHalf_GrSLType, "cornerRadius");
        proxyRectVar = args.fUniformHandler->addUniform(&_outer, kFragment_GrShaderFlag,
                                                        kFloat4_GrSLType, "proxyRect");
        blurRadiusVar = args.fUniformHandler->addUniform(&_outer, kFragment_GrShaderFlag,
                                                         kHalf_GrSLType, "blurRadius");
        fragBuilder->codeAppendf(
                R"SkSL(half2 translatedFragPos = half2(sk_FragCoord.xy - %s.xy);
half2 proxyCenter = half2((%s.zw - %s.xy) * 0.5);
half edgeSize = (2.0 * %s + %s) + 0.5;
translatedFragPos -= proxyCenter;
half2 fragDirection = sign(translatedFragPos);
translatedFragPos = abs(translatedFragPos);
translatedFragPos -= proxyCenter - edgeSize;
translatedFragPos = max(translatedFragPos, 0.0);
translatedFragPos *= fragDirection;
translatedFragPos += half2(edgeSize);
half2 proxyDims = half2(2.0 * edgeSize);
half2 texCoord = translatedFragPos / proxyDims;)SkSL",
                args.fUniformHandler->getUniformCStr(proxyRectVar),
                args.fUniformHandler->getUniformCStr(proxyRectVar),
                args.fUniformHandler->getUniformCStr(proxyRectVar),
                args.fUniformHandler->getUniformCStr(blurRadiusVar),
                args.fUniformHandler->getUniformCStr(cornerRadiusVar));
        SkString _input9600(args.fInputColor);
        SkString _sample9600;
        if (_outer.inputFP_index >= 0) {
            _sample9600 = this->invokeChild(_outer.inputFP_index, _input9600.c_str(), args);
        } else {
            _sample9600.swap(_input9600);
        }
        fragBuilder->codeAppendf(
                R"SkSL(
half4 inputColor = %s;)SkSL",
                _sample9600.c_str());
        SkString _coords9660("float2(texCoord)");
        SkString _sample9660;
        _sample9660 = this->invokeChild(_outer.ninePatchFP_index, args, _coords9660.c_str());
        fragBuilder->codeAppendf(
                R"SkSL(
%s = inputColor * %s;
)SkSL",
                args.fOutputColor, _sample9660.c_str());
    }

private:
    void onSetData(const GrGLSLProgramDataManager& pdman,
                   const GrFragmentProcessor& _proc) override {
        const GrRRectBlurEffect& _outer = _proc.cast<GrRRectBlurEffect>();
        { pdman.set1f(cornerRadiusVar, (_outer.cornerRadius)); }
        auto sigma = _outer.sigma;
        (void)sigma;
        auto rect = _outer.rect;
        (void)rect;
        UniformHandle& cornerRadius = cornerRadiusVar;
        (void)cornerRadius;
        UniformHandle& proxyRect = proxyRectVar;
        (void)proxyRect;
        UniformHandle& blurRadius = blurRadiusVar;
        (void)blurRadius;

        float blurRadiusValue = 3.f * SkScalarCeilToScalar(sigma - 1 / 6.0f);
        pdman.set1f(blurRadius, blurRadiusValue);

        SkRect outset = rect;
        outset.outset(blurRadiusValue, blurRadiusValue);
        pdman.set4f(proxyRect, outset.fLeft, outset.fTop, outset.fRight, outset.fBottom);
    }
    UniformHandle proxyRectVar;
    UniformHandle blurRadiusVar;
    UniformHandle cornerRadiusVar;
};
GrGLSLFragmentProcessor* GrRRectBlurEffect::onCreateGLSLInstance() const {
    return new GrGLSLRRectBlurEffect();
}
void GrRRectBlurEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                              GrProcessorKeyBuilder* b) const {}
bool GrRRectBlurEffect::onIsEqual(const GrFragmentProcessor& other) const {
    const GrRRectBlurEffect& that = other.cast<GrRRectBlurEffect>();
    (void)that;
    if (sigma != that.sigma) return false;
    if (rect != that.rect) return false;
    if (cornerRadius != that.cornerRadius) return false;
    return true;
}
GrRRectBlurEffect::GrRRectBlurEffect(const GrRRectBlurEffect& src)
        : INHERITED(kGrRRectBlurEffect_ClassID, src.optimizationFlags())
        , sigma(src.sigma)
        , rect(src.rect)
        , cornerRadius(src.cornerRadius) {
    if (src.inputFP_index >= 0) {
        inputFP_index = this->cloneAndRegisterChildProcessor(src.childProcessor(src.inputFP_index));
    }
    {
        ninePatchFP_index =
                this->cloneAndRegisterChildProcessor(src.childProcessor(src.ninePatchFP_index));
    }
}
std::unique_ptr<GrFragmentProcessor> GrRRectBlurEffect::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrRRectBlurEffect(*this));
}
GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrRRectBlurEffect);
#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrRRectBlurEffect::TestCreate(GrProcessorTestData* d) {
    SkScalar w = d->fRandom->nextRangeScalar(100.f, 1000.f);
    SkScalar h = d->fRandom->nextRangeScalar(100.f, 1000.f);
    SkScalar r = d->fRandom->nextRangeF(1.f, 9.f);
    SkScalar sigma = d->fRandom->nextRangeF(1.f, 10.f);
    SkRRect rrect;
    rrect.setRectXY(SkRect::MakeWH(w, h), r, r);
    return GrRRectBlurEffect::Make(/*inputFP=*/nullptr, d->context(), sigma, sigma, rrect, rrect);
}
#endif
