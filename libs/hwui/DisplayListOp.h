/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HWUI_DISPLAY_OPERATION_H
#define ANDROID_HWUI_DISPLAY_OPERATION_H

#ifndef LOG_TAG
    #define LOG_TAG "OpenGLRenderer"
#endif

#include <SkColor.h>
#include <SkPath.h>
#include <SkPathOps.h>
#include <SkXfermode.h>

#include <private/hwui/DrawGlInfo.h>

#include "OpenGLRenderer.h"
#include "AssetAtlas.h"
#include "DeferredDisplayList.h"
#include "DisplayListRenderer.h"
#include "UvMapper.h"
#include "utils/LinearAllocator.h"

#define CRASH() do { \
    *(int *)(uintptr_t) 0xbbadbeef = 0; \
    ((void(*)())0)(); /* More reliable, but doesn't say BBADBEEF */ \
} while(false)

// Use OP_LOG for logging with arglist, OP_LOGS if just printing char*
#define OP_LOGS(s) OP_LOG("%s", (s))
#define OP_LOG(s, ...) ALOGD( "%*s" s, level * 2, "", __VA_ARGS__ )
//ligengchao start
//#define TAG    "ligengchao RenderNode" // 这个是自定义的LOG的标识  
//#define LOGD(...)  //__android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__) // 定义LOGD类型  
//ligengchao end

namespace android {
namespace uirenderer {

/**
 * Structure for storing canvas operations when they are recorded into a DisplayList, so that they
 * may be replayed to an OpenGLRenderer.
 *
 * To avoid individual memory allocations, DisplayListOps may only be allocated into a
 * LinearAllocator's managed memory buffers.  Each pointer held by a DisplayListOp is either a
 * pointer into memory also allocated in the LinearAllocator (mostly for text and float buffers) or
 * references a externally refcounted object (Sk... and Skia... objects). ~DisplayListOp() is
 * never called as LinearAllocators are simply discarded, so no memory management should be done in
 * this class.
 */
 //ligengchao start
 enum DisplayListOpType {
        DisplayListOpT,
        StateOpT,
        DrawOpT,
        DrawBoundedOpT,
        SaveOpT,
        RestoreToCountOpT,
        SaveLayerOpT,
        TranslateOpT,
        RotateOpT,
        ScaleOpT,
        SkewOpT,
        SetMatrixOpT,
        ConcatMatrixOpT,
        ClipOpT,
        ClipRectOpT,
        ClipPathOpT,
        ClipRegionOpT,
        ResetPaintFilterOpT,
        SetupPaintFilterOpT,
        DrawBitmapOpT,
        DrawBitmapRectOpT,
        DrawBitmapDataOpT,
        DrawBitmapMeshOpT,
        DrawPatchOpT,
        DrawColorOpT,
        DrawStrokableOpT,
        DrawRectOpT,
        DrawRectsOpT,
        DrawRoundRectOpT,
        DrawRoundRectPropsOpT,
        DrawCircleOpT,
        DrawCirclePropsOpT,
        DrawOvalOpT,
        DrawArcOpT,
        DrawPathOpT,
        DrawLinesOpT,
        DrawPointsOpT,
        DrawSomeTextOpT,
        DrawTextOnPathOpT,
        DrawPosTextOpT,
        DrawTextOpT,
        DrawFunctorOpT,
        DrawRenderNodeOpT,
        DrawShadowOpT,
        DrawLayerOpT,
    };
//ligengchao end

class DisplayListOp {
public:
    // These objects should always be allocated with a LinearAllocator, and never destroyed/deleted.
    // standard new() intentionally not implemented, and delete/deconstructor should never be used.
	DisplayListOp() {  type = DisplayListOpT;}//ligengchao

	virtual ~DisplayListOp() { CRASH(); }
    static void operator delete(void* ptr) { CRASH(); }
    /** static void* operator new(size_t size); PURPOSELY OMITTED **/
    static void* operator new(size_t size, LinearAllocator& allocator) {
        return allocator.alloc(size);
    }

    enum OpLogFlag {
        kOpLogFlag_Recurse = 0x1,
        kOpLogFlag_JSON = 0x2 // TODO: add?
    };

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) = 0;

    virtual void replay(ReplayStateStruct& replayStruct, int saveCount, int level,
            bool useQuickReject) = 0;

    virtual void output(int level, uint32_t logFlags = 0) const = 0;

    // NOTE: it would be nice to declare constants and overriding the implementation in each op to
    // point at the constants, but that seems to require a .cpp file
    virtual const char* name() = 0;

	//ligengchao start

	virtual bool equal(DisplayListOp* DisplayListOp2){
		return false;
	}
	
	virtual bool equal2(DisplayListOp* DisplayListOp2){
		return type == DisplayListOp2->type;
	}


	int type;
	//ligengchao end
};

class StateOp : public DisplayListOp {
public:
    StateOp() {
		type = StateOpT;//ligengchao 
	}

    virtual ~StateOp() {}

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        // default behavior only affects immediate, deferrable state, issue directly to renderer
        applyState(deferStruct.mRenderer, saveCount);
    }

    /**
     * State operations are applied directly to the renderer, but can cause the deferred drawing op
     * list to flush
     */
    virtual void replay(ReplayStateStruct& replayStruct, int saveCount, int level,
            bool useQuickReject) {
        applyState(replayStruct.mRenderer, saveCount);
    }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const = 0;

	//ligengchao start

	virtual bool equal2(DisplayListOp* DisplayListOp2){
		return type == DisplayListOp2->type;
	}
	//ligengchao end
};

class DrawOp : public DisplayListOp {
friend class MergingDrawBatch;
public:
    DrawOp(const SkPaint* paint)
            : mPaint(paint), mQuickRejected(false) {
		type = DrawOpT;//ligengchao 
	}

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        if (mQuickRejected && CC_LIKELY(useQuickReject)) {
            return;
        }

        deferStruct.mDeferredList.addDrawOp(deferStruct.mRenderer, this);
    }

    virtual void replay(ReplayStateStruct& replayStruct, int saveCount, int level,
            bool useQuickReject) {
        if (mQuickRejected && CC_LIKELY(useQuickReject)) {
            return;
        }

        replayStruct.mDrawGlStatus |= applyDraw(replayStruct.mRenderer, replayStruct.mDirty);
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) = 0;

    /**
     * Draw multiple instances of an operation, must be overidden for operations that merge
     *
     * Currently guarantees certain similarities between ops (see MergingDrawBatch::canMergeWith),
     * and pure translation transformations. Other guarantees of similarity should be enforced by
     * reducing which operations are tagged as mergeable.
     */
    virtual status_t multiDraw(OpenGLRenderer& renderer, Rect& dirty,
            const Vector<OpStatePair>& ops, const Rect& bounds) {
        status_t status = DrawGlInfo::kStatusDone;
        for (unsigned int i = 0; i < ops.size(); i++) {
            renderer.restoreDisplayState(*(ops[i].state), true);
            status |= ops[i].op->applyDraw(renderer, dirty);
        }
        return status;
    }

    /**
     * When this method is invoked the state field is initialized to have the
     * final rendering state. We can thus use it to process data as it will be
     * used at draw time.
     *
     * Additionally, this method allows subclasses to provide defer-time preferences for batching
     * and merging.
     *
     * if a subclass can set deferInfo.mergeable to true, it should implement multiDraw()
     */
    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {}

    /**
     * Query the conservative, local bounds (unmapped) bounds of the op.
     *
     * returns true if bounds exist
     */
    virtual bool getLocalBounds(Rect& localBounds) {
        return false;
    }

    // TODO: better refine localbounds usage
    void setQuickRejected(bool quickRejected) { mQuickRejected = quickRejected; }
    bool getQuickRejected() { return mQuickRejected; }

    inline int getPaintAlpha() const {
        return OpenGLRenderer::getAlphaDirect(mPaint);
    }

    virtual bool hasTextShadow() const {
        return false;
    }

    inline float strokeWidthOutset() {
        // since anything AA stroke with less than 1.0 pixel width is drawn with an alpha-reduced
        // 1.0 stroke, treat 1.0 as minimum.

        // TODO: it would be nice if this could take scale into account, but scale isn't stable
        // since higher levels of the view hierarchy can change scale out from underneath it.
        return fmaxf(mPaint->getStrokeWidth(), 1) * 0.5f;
    }


bool compare(const SkPaint& a, const SkPaint& b) {
//#define EQUAL(field) (a.field == b.field)
	// Don't check fGenerationID or fDirtyBits, which can be different for logically equal paints.
	if(&a != NULL && &b != NULL)
		return a.getColor() == b.getColor();
	return false;
//	EQUAL(getTypeface())
//		&& EQUAL(getPathEffect())
//		&& EQUAL(getShader())
//		&& EQUAL(getXfermode())
//		&& EQUAL(getMaskFilter())
//		&& EQUAL(getColorFilter())
//		&& EQUAL(getRasterizer())
//		&& EQUAL(getLooper())
//		&& EQUAL(getImageFilter())
//		&& EQUAL(getAnnotation())
//		 EQUAL(getTextSize())
//		&& EQUAL(getTextScaleX())
//		&& EQUAL(getTextSkewX())
//		&& EQUAL(getColor())
//		&& EQUAL(getStrokeWidth())
//		&& EQUAL(getStrokeMiter())
		//&& EQUAL(getBitfields())

//		&& EQUAL(fPaintOptionsAndroid)

//		&& EQUAL(fpPaintOptionsAndroid)

		;
//#undef EQUAL
}

//ligengchao start
	const SkPaint* getmPaint() const { return mPaint; }
	bool getmQuickRejected() const { return mQuickRejected; }

	virtual bool equal2(const DisplayListOp* DisplayListOp2){
//		LOGD("DrawOp: %d %d %d %d ",type == DisplayListOp2->type, getmQuickRejected() == 
//			((DrawOp*)DisplayListOp2)->getmQuickRejected(), getmPaint() 
//			 , (((DrawOp*)DisplayListOp2)->getmPaint()));
		if(type == DisplayListOp2->type && getmQuickRejected() == ((DrawOp*)DisplayListOp2)->getmQuickRejected() && getmPaint() 
			 && (((DrawOp*)DisplayListOp2)->getmPaint()) && *getmPaint() == *(((DrawOp*)DisplayListOp2)->getmPaint()))
			return true;
		return false;
	}

//ligengchao end

protected:
    const SkPaint* getPaint(OpenGLRenderer& renderer) {
        return renderer.filterPaint(mPaint);
    }

    // Helper method for determining op opaqueness. Assumes op fills its bounds in local
    // coordinates, and that paint's alpha is used
    inline bool isOpaqueOverBounds(const DeferredDisplayState& state) {
        // ensure that local bounds cover mapped bounds
        if (!state.mMatrix.isSimple()) return false;

        if (state.mRoundRectClipState) return false;

        // check state/paint for transparency
        if (mPaint) {
            if (mPaint->getShader() && !mPaint->getShader()->isOpaque()) {
                return false;
            }
            if (mPaint->getAlpha() != 0xFF) {
                return false;
            }
        }

        if (state.mAlpha != 1.0f) return false;

        SkXfermode::Mode mode = OpenGLRenderer::getXfermodeDirect(mPaint);
        return (mode == SkXfermode::kSrcOver_Mode ||
                mode == SkXfermode::kSrc_Mode);

    }

    const SkPaint* mPaint; // should be accessed via getPaint() when applying
    bool mQuickRejected;
};

class DrawBoundedOp : public DrawOp {
public:
    DrawBoundedOp(float left, float top, float right, float bottom, const SkPaint* paint)
            : DrawOp(paint), mLocalBounds(left, top, right, bottom) {
		type = DrawBoundedOpT;//ligengchao 
	}

    DrawBoundedOp(const Rect& localBounds, const SkPaint* paint)
            : DrawOp(paint), mLocalBounds(localBounds) {
		type = DrawBoundedOpT;//ligengchao 
	}

    // Calculates bounds as smallest rect encompassing all points
    // NOTE: requires at least 1 vertex, and doesn't account for stroke size (should be handled in
    // subclass' constructor)
    DrawBoundedOp(const float* points, int count, const SkPaint* paint)
            : DrawOp(paint), mLocalBounds(points[0], points[1], points[0], points[1]) {
		type = DrawBoundedOpT;//ligengchao 
        for (int i = 2; i < count; i += 2) {
            mLocalBounds.left = fminf(mLocalBounds.left, points[i]);
            mLocalBounds.right = fmaxf(mLocalBounds.right, points[i]);
            mLocalBounds.top = fminf(mLocalBounds.top, points[i + 1]);
            mLocalBounds.bottom = fmaxf(mLocalBounds.bottom, points[i + 1]);
        }
    }

    // default empty constructor for bounds, to be overridden in child constructor body
    DrawBoundedOp(const SkPaint* paint): DrawOp(paint) {
		type = DrawBoundedOpT;//ligengchao 
	}

    virtual bool getLocalBounds(Rect& localBounds) {
        localBounds.set(mLocalBounds);
        OpenGLRenderer::TextShadow textShadow;
        if (OpenGLRenderer::getTextShadow(mPaint, &textShadow)) {
            Rect shadow(mLocalBounds);
            shadow.translate(textShadow.dx, textShadow.dx);
            shadow.outset(textShadow.radius);
            localBounds.unionWith(shadow);
        }
        return true;
    }
	//ligengchao start

	Rect getmLocalBounds() const { return mLocalBounds; }//ligengchao
	virtual bool equal2(DisplayListOp* DisplayListOp2){
//		LOGD("DrawBoundedOp: %d %f %f %f %f %f %f %f %f ",this->DrawOp::equal2(DisplayListOp2), 
//			getmLocalBounds().left, getmLocalBounds().right, getmLocalBounds().top, getmLocalBounds().bottom, 
//			((DrawBoundedOp*)DisplayListOp2)->getmLocalBounds().left, ((DrawBoundedOp*)DisplayListOp2)->getmLocalBounds().right,
//			((DrawBoundedOp*)DisplayListOp2)->getmLocalBounds().top, ((DrawBoundedOp*)DisplayListOp2)->getmLocalBounds().bottom);
		if(this->DrawOp::equal2(DisplayListOp2) && getmLocalBounds() == ((DrawBoundedOp*)DisplayListOp2)->getmLocalBounds())
			return true;
		return false;
	}
	//ligengchao end

protected:
    Rect mLocalBounds; // displayed area in LOCAL coord. doesn't incorporate stroke, so check paint
};

///////////////////////////////////////////////////////////////////////////////
// STATE OPERATIONS - these may affect the state of the canvas/renderer, but do
//         not directly draw or alter output
///////////////////////////////////////////////////////////////////////////////

class SaveOp : public StateOp {
public:
    SaveOp(int flags)
            : mFlags(flags) {
		type = SaveOpT;//ligengchao 
	}

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        int newSaveCount = deferStruct.mRenderer.save(mFlags);
        deferStruct.mDeferredList.addSave(deferStruct.mRenderer, this, newSaveCount);
    }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.save(mFlags);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Save flags %x", mFlags);
    }

    virtual const char* name() { return "Save"; }

    int getFlags() const { return mFlags; }

	//ligengchao start
	virtual bool equal(DisplayListOp* DisplayListOp2){
		if(type == DisplayListOp2->type && getFlags() ==  ((SaveOp*)DisplayListOp2)->getFlags())
			return true;
		return false;
	}
	//ligengchao end
private:
    int mFlags;
};

class RestoreToCountOp : public StateOp {
public:
    RestoreToCountOp(int count)
            : mCount(count) {
		type = RestoreToCountOpT;//ligengchao 
	}
    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        deferStruct.mDeferredList.addRestoreToCount(deferStruct.mRenderer,
                this, saveCount + mCount);
        deferStruct.mRenderer.restoreToCount(saveCount + mCount);
    }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.restoreToCount(saveCount + mCount);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Restore to count %d", mCount);
    }

    virtual const char* name() { return "RestoreToCount"; }
	//ligengchao start
	int getmCount() const { return mCount; }
	virtual bool equal(DisplayListOp* DisplayListOp2){
		if(type == DisplayListOp2->type && getmCount() == ((RestoreToCountOp*)DisplayListOp2)->getmCount())
			return true;
		return false;
	}
	//ligengchao end


private:
    int mCount;
};

class SaveLayerOp : public StateOp {
public:
    SaveLayerOp(float left, float top, float right, float bottom, int alpha, int flags)
            : mArea(left, top, right, bottom)
            , mPaint(&mCachedPaint)
            , mFlags(flags)
            , mConvexMask(NULL) {
		type = SaveLayerOpT;//ligengchao 
        mCachedPaint.setAlpha(alpha);
    }

    SaveLayerOp(float left, float top, float right, float bottom, const SkPaint* paint, int flags)
            : mArea(left, top, right, bottom)
            , mPaint(paint)
            , mFlags(flags)
            , mConvexMask(NULL){
		type = SaveLayerOpT;//ligengchao 
   }

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        // NOTE: don't bother with actual saveLayer, instead issuing it at flush time
        int newSaveCount = deferStruct.mRenderer.getSaveCount();
        deferStruct.mDeferredList.addSaveLayer(deferStruct.mRenderer, this, newSaveCount);

        // NOTE: don't issue full saveLayer, since that has side effects/is costly. instead just
        // setup the snapshot for deferral, and re-issue the op at flush time
        deferStruct.mRenderer.saveLayerDeferred(mArea.left, mArea.top, mArea.right, mArea.bottom,
                mPaint, mFlags);
    }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.saveLayer(mArea.left, mArea.top, mArea.right, mArea.bottom,
                mPaint, mFlags, mConvexMask);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("SaveLayer%s of area " RECT_STRING,
                (isSaveLayerAlpha() ? "Alpha" : ""),RECT_ARGS(mArea));
    }

    virtual const char* name() { return isSaveLayerAlpha() ? "SaveLayerAlpha" : "SaveLayer"; }

    int getFlags() { return mFlags; }

    // Called to make SaveLayerOp clip to the provided mask when drawing back/restored
    void setMask(const SkPath* convexMask) {
        mConvexMask = convexMask;
    }

private:
    bool isSaveLayerAlpha() const {
        SkXfermode::Mode mode = OpenGLRenderer::getXfermodeDirect(mPaint);
        int alpha = OpenGLRenderer::getAlphaDirect(mPaint);
        return alpha < 255 && mode == SkXfermode::kSrcOver_Mode;
    }

    Rect mArea;
    const SkPaint* mPaint;
    SkPaint mCachedPaint;
    int mFlags;

    // Convex path, points at data in RenderNode, valid for the duration of the frame only
    // Only used for masking the SaveLayer which wraps projected RenderNodes
    const SkPath* mConvexMask;
};

class TranslateOp : public StateOp {
public:
    TranslateOp(float dx, float dy)
            : mDx(dx), mDy(dy) {
		type = TranslateOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.translate(mDx, mDy);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Translate by %f %f", mDx, mDy);
    }

    virtual const char* name() { return "Translate"; }

private:
    float mDx;
    float mDy;
};

class RotateOp : public StateOp {
public:
    RotateOp(float degrees)
            : mDegrees(degrees){
		type = RotateOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.rotate(mDegrees);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Rotate by %f degrees", mDegrees);
    }

    virtual const char* name() { return "Rotate"; }

private:
    float mDegrees;
};

class ScaleOp : public StateOp {
public:
    ScaleOp(float sx, float sy)
            : mSx(sx), mSy(sy){
		type = ScaleOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.scale(mSx, mSy);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Scale by %f %f", mSx, mSy);
    }

    virtual const char* name() { return "Scale"; }

private:
    float mSx;
    float mSy;
};

class SkewOp : public StateOp {
public:
    SkewOp(float sx, float sy)
            : mSx(sx), mSy(sy) {
		type = SkewOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.skew(mSx, mSy);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Skew by %f %f", mSx, mSy);
    }

    virtual const char* name() { return "Skew"; }

private:
    float mSx;
    float mSy;
};

class SetMatrixOp : public StateOp {
public:
    SetMatrixOp(const SkMatrix& matrix)
            : mMatrix(matrix) {
		type = SetMatrixOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.setMatrix(mMatrix);
    }

    virtual void output(int level, uint32_t logFlags) const {
        if (mMatrix.isIdentity()) {
            OP_LOGS("SetMatrix (reset)");
        } else {
            OP_LOG("SetMatrix " SK_MATRIX_STRING, SK_MATRIX_ARGS(&mMatrix));
        }
    }

    virtual const char* name() { return "SetMatrix"; }

private:
    const SkMatrix mMatrix;
};

class ConcatMatrixOp : public StateOp {
public:
    ConcatMatrixOp(const SkMatrix& matrix)
            : mMatrix(matrix){
		type = ConcatMatrixOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.concatMatrix(mMatrix);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("ConcatMatrix " SK_MATRIX_STRING, SK_MATRIX_ARGS(&mMatrix));
    }

    virtual const char* name() { return "ConcatMatrix"; }

private:
    const SkMatrix mMatrix;
};

class ClipOp : public StateOp {
public:
    ClipOp(SkRegion::Op op) : mOp(op){
		type = ClipOpT;//ligengchao 
   }

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        // NOTE: must defer op BEFORE applying state, since it may read clip
        deferStruct.mDeferredList.addClip(deferStruct.mRenderer, this);

        // TODO: Can we avoid applying complex clips at defer time?
        applyState(deferStruct.mRenderer, saveCount);
    }

    bool canCauseComplexClip() {
        return ((mOp != SkRegion::kIntersect_Op) && (mOp != SkRegion::kReplace_Op)) || !isRect();
    }

	SkRegion::Op getmOp() const { return mOp; }//ligengchao
	

protected:
    virtual bool isRect() { return false; }

    SkRegion::Op mOp;
};

class ClipRectOp : public ClipOp {
public:
    ClipRectOp(float left, float top, float right, float bottom, SkRegion::Op op)
            : ClipOp(op), mArea(left, top, right, bottom) {
		type = ClipRectOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.clipRect(mArea.left, mArea.top, mArea.right, mArea.bottom, mOp);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("ClipRect " RECT_STRING, RECT_ARGS(mArea));
    }

    virtual const char* name() { return "ClipRect"; }

	//ligengchao start
	virtual bool equal(DisplayListOp* DisplayListOp2){
//		LOGD("ClipRectOp: %d %d", getArea() == ((ClipRectOp*)DisplayListOp2)->getArea()
//			, getmOp() == ((ClipOp*)DisplayListOp2)->getmOp());
		if(type == DisplayListOp2->type && getArea() == ((ClipRectOp*)DisplayListOp2)->getArea() && 
			getmOp() == ((ClipOp*)DisplayListOp2)->getmOp())
			return true;
		return false;
	}

	const Rect& getArea() const { return mArea; }//ligengchao
	//ligengchao end

protected:
    virtual bool isRect() { return true; }

private:
    Rect mArea;
};

class ClipPathOp : public ClipOp {
public:
    ClipPathOp(const SkPath* path, SkRegion::Op op)
            : ClipOp(op), mPath(path) {
		type =ClipPathOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.clipPath(mPath, mOp);
    }

    virtual void output(int level, uint32_t logFlags) const {
        SkRect bounds = mPath->getBounds();
        OP_LOG("ClipPath bounds " RECT_STRING,
                bounds.left(), bounds.top(), bounds.right(), bounds.bottom());
    }

    virtual const char* name() { return "ClipPath"; }

private:
    const SkPath* mPath;
};

class ClipRegionOp : public ClipOp {
public:
    ClipRegionOp(const SkRegion* region, SkRegion::Op op)
            : ClipOp(op), mRegion(region) {
		type = ClipRegionOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.clipRegion(mRegion, mOp);
    }

    virtual void output(int level, uint32_t logFlags) const {
        SkIRect bounds = mRegion->getBounds();
        OP_LOG("ClipRegion bounds %d %d %d %d",
                bounds.left(), bounds.top(), bounds.right(), bounds.bottom());
    }

    virtual const char* name() { return "ClipRegion"; }

private:
    const SkRegion* mRegion;
};

class ResetPaintFilterOp : public StateOp {
public:
	ResetPaintFilterOp(){//ligengchao 
		type = ResetPaintFilterOpT;//ligengchao 
   }
    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.resetPaintFilter();
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOGS("ResetPaintFilter");
    }

    virtual const char* name() { return "ResetPaintFilter"; }
};

class SetupPaintFilterOp : public StateOp {
public:
    SetupPaintFilterOp(int clearBits, int setBits)
            : mClearBits(clearBits), mSetBits(setBits) {
		type = SetupPaintFilterOpT;//ligengchao 
   }

    virtual void applyState(OpenGLRenderer& renderer, int saveCount) const {
        renderer.setupPaintFilter(mClearBits, mSetBits);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("SetupPaintFilter, clear %#x, set %#x", mClearBits, mSetBits);
    }

    virtual const char* name() { return "SetupPaintFilter"; }

private:
    int mClearBits;
    int mSetBits;
};

///////////////////////////////////////////////////////////////////////////////
// DRAW OPERATIONS - these are operations that can draw to the canvas's device
///////////////////////////////////////////////////////////////////////////////

class DrawBitmapOp : public DrawBoundedOp {
public:
    DrawBitmapOp(const SkBitmap* bitmap, const SkPaint* paint)
            : DrawBoundedOp(0, 0, bitmap->width(), bitmap->height(), paint)
            , mBitmap(bitmap)
            , mAtlas(Caches::getInstance().assetAtlas) {
		type = DrawBitmapOpT;//ligengchao 
        mEntry = mAtlas.getEntry(bitmap);
        if (mEntry) {
            mEntryGenerationId = mAtlas.getGenerationId();
            mUvMapper = mEntry->uvMapper;
        }
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawBitmap(mBitmap, getPaint(renderer));
    }

    AssetAtlas::Entry* getAtlasEntry() {
        // The atlas entry is stale, let's get a new one
        if (mEntry && mEntryGenerationId != mAtlas.getGenerationId()) {
            mEntryGenerationId = mAtlas.getGenerationId();
            mEntry = mAtlas.getEntry(mBitmap);
            mUvMapper = mEntry->uvMapper;
        }
        return mEntry;
    }

#define SET_TEXTURE(ptr, posRect, offsetRect, texCoordsRect, xDim, yDim) \
    TextureVertex::set(ptr++, posRect.xDim - offsetRect.left, posRect.yDim - offsetRect.top, \
            texCoordsRect.xDim, texCoordsRect.yDim)

    /**
     * This multi-draw operation builds a mesh on the stack by generating a quad
     * for each bitmap in the batch. This method is also responsible for dirtying
     * the current layer, if any.
     */
    virtual status_t multiDraw(OpenGLRenderer& renderer, Rect& dirty,
            const Vector<OpStatePair>& ops, const Rect& bounds) {
        const DeferredDisplayState& firstState = *(ops[0].state);
        renderer.restoreDisplayState(firstState, true); // restore all but the clip

        TextureVertex vertices[6 * ops.size()];
        TextureVertex* vertex = &vertices[0];

        const bool hasLayer = renderer.hasLayer();
        bool pureTranslate = true;

        // TODO: manually handle rect clip for bitmaps by adjusting texCoords per op,
        // and allowing them to be merged in getBatchId()
        for (unsigned int i = 0; i < ops.size(); i++) {
            const DeferredDisplayState& state = *(ops[i].state);
            const Rect& opBounds = state.mBounds;
            // When we reach multiDraw(), the matrix can be either
            // pureTranslate or simple (translate and/or scale).
            // If the matrix is not pureTranslate, then we have a scale
            pureTranslate &= state.mMatrix.isPureTranslate();

            Rect texCoords(0, 0, 1, 1);
            ((DrawBitmapOp*) ops[i].op)->mUvMapper.map(texCoords);

            SET_TEXTURE(vertex, opBounds, bounds, texCoords, left, top);
            SET_TEXTURE(vertex, opBounds, bounds, texCoords, right, top);
            SET_TEXTURE(vertex, opBounds, bounds, texCoords, left, bottom);

            SET_TEXTURE(vertex, opBounds, bounds, texCoords, left, bottom);
            SET_TEXTURE(vertex, opBounds, bounds, texCoords, right, top);
            SET_TEXTURE(vertex, opBounds, bounds, texCoords, right, bottom);

            if (hasLayer) {
                renderer.dirtyLayer(opBounds.left, opBounds.top, opBounds.right, opBounds.bottom);
            }
        }

        return renderer.drawBitmaps(mBitmap, mEntry, ops.size(), &vertices[0],
                pureTranslate, bounds, mPaint);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw bitmap %p at %f %f", mBitmap, mLocalBounds.left, mLocalBounds.top);
    }

    virtual const char* name() { return "DrawBitmap"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Bitmap;
        deferInfo.mergeId = getAtlasEntry() ?
                (mergeid_t) mEntry->getMergeId() : (mergeid_t) mBitmap;

        // Don't merge non-simply transformed or neg scale ops, SET_TEXTURE doesn't handle rotation
        // Don't merge A8 bitmaps - the paint's color isn't compared by mergeId, or in
        // MergingDrawBatch::canMergeWith()
        // TODO: support clipped bitmaps by handling them in SET_TEXTURE
        deferInfo.mergeable = state.mMatrix.isSimple() && state.mMatrix.positiveScale() &&
                !state.mClipSideFlags &&
                OpenGLRenderer::getXfermodeDirect(mPaint) == SkXfermode::kSrcOver_Mode &&
                (mBitmap->colorType() != kAlpha_8_SkColorType);
    }

    const SkBitmap* bitmap() { return mBitmap; }

//	//ligengchao start
//	bool compareBitmap(const SkBitmap1*, const SkBitmap2*){
//		LOGD("DrawBitmapOp: %d %d %d %d",this->DrawBoundedOp::equal2(DisplayListOp2), bitmap()->info() == ((DrawBitmapOp*)DisplayListOp2)->bitmap()->info()
//			, !memcmp(bitmap()->getPixels(), ((DrawBitmapOp*)DisplayListOp2)->bitmap()->getPixels(), bitmap()->getSize()), bitmap()->getSize()

//		return !memcmp(bitmap()->getPixels(), ((DrawBitmapOp*)DisplayListOp2)->bitmap()->getPixels(), bitmap->getSize())
//			&& bitmap()->info() == ((DrawBitmapOp*)DisplayListOp2)->bitmap()->info();

//		
//	}
	
	virtual bool equal(DisplayListOp* DisplayListOp2){
//		LOGD("DrawBitmapOp: %d %d",this->DrawBoundedOp::equal2(DisplayListOp2)
//			, bitmap() == ((DrawBitmapOp*)DisplayListOp2)->bitmap());
		if(this->DrawBoundedOp::equal2(DisplayListOp2) && bitmap() == ((DrawBitmapOp*)DisplayListOp2)->bitmap())
			return true;
		return false;
	}
	//ligengchao end
protected:
    const SkBitmap* mBitmap;
    const AssetAtlas& mAtlas;
    uint32_t mEntryGenerationId;
    AssetAtlas::Entry* mEntry;
    UvMapper mUvMapper;
};

class DrawBitmapRectOp : public DrawBoundedOp {
public:
    DrawBitmapRectOp(const SkBitmap* bitmap,
            float srcLeft, float srcTop, float srcRight, float srcBottom,
            float dstLeft, float dstTop, float dstRight, float dstBottom, const SkPaint* paint)
            : DrawBoundedOp(dstLeft, dstTop, dstRight, dstBottom, paint),
            mBitmap(bitmap), mSrc(srcLeft, srcTop, srcRight, srcBottom) {
		type = DrawBitmapRectOpT;//ligengchao 
   }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawBitmap(mBitmap, mSrc.left, mSrc.top, mSrc.right, mSrc.bottom,
                mLocalBounds.left, mLocalBounds.top, mLocalBounds.right, mLocalBounds.bottom,
                getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw bitmap %p src=" RECT_STRING ", dst=" RECT_STRING,
                mBitmap, RECT_ARGS(mSrc), RECT_ARGS(mLocalBounds));
    }

    virtual const char* name() { return "DrawBitmapRect"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Bitmap;
    }

private:
    const SkBitmap* mBitmap;
    Rect mSrc;
};

class DrawBitmapDataOp : public DrawBitmapOp {
public:
    DrawBitmapDataOp(const SkBitmap* bitmap, const SkPaint* paint)
            : DrawBitmapOp(bitmap, paint) {
		type =DrawBitmapDataOpT;//ligengchao 
   }
    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawBitmapData(mBitmap, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw bitmap %p", mBitmap);
    }

    virtual const char* name() { return "DrawBitmapData"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Bitmap;
    }
};

class DrawBitmapMeshOp : public DrawBoundedOp {
public:
    DrawBitmapMeshOp(const SkBitmap* bitmap, int meshWidth, int meshHeight,
            const float* vertices, const int* colors, const SkPaint* paint)
            : DrawBoundedOp(vertices, 2 * (meshWidth + 1) * (meshHeight + 1), paint),
            mBitmap(bitmap), mMeshWidth(meshWidth), mMeshHeight(meshHeight),
            mVertices(vertices), mColors(colors) {
		type = DrawBitmapMeshOpT;//ligengchao 
   }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawBitmapMesh(mBitmap, mMeshWidth, mMeshHeight,
                mVertices, mColors, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw bitmap %p mesh %d x %d", mBitmap, mMeshWidth, mMeshHeight);
    }

    virtual const char* name() { return "DrawBitmapMesh"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Bitmap;
    }

private:
    const SkBitmap* mBitmap;
    int mMeshWidth;
    int mMeshHeight;
    const float* mVertices;
    const int* mColors;
};

class DrawPatchOp : public DrawBoundedOp {
public:
    DrawPatchOp(const SkBitmap* bitmap, const Res_png_9patch* patch,
            float left, float top, float right, float bottom, const SkPaint* paint)
            : DrawBoundedOp(left, top, right, bottom, paint),
            mBitmap(bitmap), mPatch(patch), mGenerationId(0), mMesh(NULL),
            mAtlas(Caches::getInstance().assetAtlas) {
		type = DrawPatchOpT;//ligengchao 
        mEntry = mAtlas.getEntry(bitmap);
        if (mEntry) {
            mEntryGenerationId = mAtlas.getGenerationId();
        }
    };

    AssetAtlas::Entry* getAtlasEntry() {
        // The atlas entry is stale, let's get a new one
        if (mEntry && mEntryGenerationId != mAtlas.getGenerationId()) {
            mEntryGenerationId = mAtlas.getGenerationId();
            mEntry = mAtlas.getEntry(mBitmap);
        }
        return mEntry;
    }

    const Patch* getMesh(OpenGLRenderer& renderer) {
        if (!mMesh || renderer.getCaches().patchCache.getGenerationId() != mGenerationId) {
            PatchCache& cache = renderer.getCaches().patchCache;
            mMesh = cache.get(getAtlasEntry(), mBitmap->width(), mBitmap->height(),
                    mLocalBounds.getWidth(), mLocalBounds.getHeight(), mPatch);
            mGenerationId = cache.getGenerationId();
        }
        return mMesh;
    }

    /**
     * This multi-draw operation builds an indexed mesh on the stack by copying
     * and transforming the vertices of each 9-patch in the batch. This method
     * is also responsible for dirtying the current layer, if any.
     */
    virtual status_t multiDraw(OpenGLRenderer& renderer, Rect& dirty,
            const Vector<OpStatePair>& ops, const Rect& bounds) {
        const DeferredDisplayState& firstState = *(ops[0].state);
        renderer.restoreDisplayState(firstState, true); // restore all but the clip

        // Batches will usually contain a small number of items so it's
        // worth performing a first iteration to count the exact number
        // of vertices we need in the new mesh
        uint32_t totalVertices = 0;
        for (unsigned int i = 0; i < ops.size(); i++) {
            totalVertices += ((DrawPatchOp*) ops[i].op)->getMesh(renderer)->verticesCount;
        }

        const bool hasLayer = renderer.hasLayer();

        uint32_t indexCount = 0;

        TextureVertex vertices[totalVertices];
        TextureVertex* vertex = &vertices[0];

        // Create a mesh that contains the transformed vertices for all the
        // 9-patch objects that are part of the batch. Note that onDefer()
        // enforces ops drawn by this function to have a pure translate or
        // identity matrix
        for (unsigned int i = 0; i < ops.size(); i++) {
            DrawPatchOp* patchOp = (DrawPatchOp*) ops[i].op;
            const DeferredDisplayState* state = ops[i].state;
            const Patch* opMesh = patchOp->getMesh(renderer);
            uint32_t vertexCount = opMesh->verticesCount;
            if (vertexCount == 0) continue;

            // We use the bounds to know where to translate our vertices
            // Using patchOp->state.mBounds wouldn't work because these
            // bounds are clipped
            const float tx = (int) floorf(state->mMatrix.getTranslateX() +
                    patchOp->mLocalBounds.left + 0.5f);
            const float ty = (int) floorf(state->mMatrix.getTranslateY() +
                    patchOp->mLocalBounds.top + 0.5f);

            // Copy & transform all the vertices for the current operation
            TextureVertex* opVertices = opMesh->vertices;
            for (uint32_t j = 0; j < vertexCount; j++, opVertices++) {
                TextureVertex::set(vertex++,
                        opVertices->x + tx, opVertices->y + ty,
                        opVertices->u, opVertices->v);
            }

            // Dirty the current layer if possible. When the 9-patch does not
            // contain empty quads we can take a shortcut and simply set the
            // dirty rect to the object's bounds.
            if (hasLayer) {
                if (!opMesh->hasEmptyQuads) {
                    renderer.dirtyLayer(tx, ty,
                            tx + patchOp->mLocalBounds.getWidth(),
                            ty + patchOp->mLocalBounds.getHeight());
                } else {
                    const size_t count = opMesh->quads.size();
                    for (size_t i = 0; i < count; i++) {
                        const Rect& quadBounds = opMesh->quads[i];
                        const float x = tx + quadBounds.left;
                        const float y = ty + quadBounds.top;
                        renderer.dirtyLayer(x, y,
                                x + quadBounds.getWidth(), y + quadBounds.getHeight());
                    }
                }
            }

            indexCount += opMesh->indexCount;
        }

        return renderer.drawPatches(mBitmap, getAtlasEntry(),
                &vertices[0], indexCount, getPaint(renderer));
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        // We're not calling the public variant of drawPatch() here
        // This method won't perform the quickReject() since we've already done it at this point
        return renderer.drawPatch(mBitmap, getMesh(renderer), getAtlasEntry(),
                mLocalBounds.left, mLocalBounds.top, mLocalBounds.right, mLocalBounds.bottom,
                getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw patch " RECT_STRING, RECT_ARGS(mLocalBounds));
    }

    virtual const char* name() { return "DrawPatch"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Patch;
        deferInfo.mergeId = getAtlasEntry() ? (mergeid_t) mEntry->getMergeId() : (mergeid_t) mBitmap;
        deferInfo.mergeable = state.mMatrix.isPureTranslate() &&
                OpenGLRenderer::getXfermodeDirect(mPaint) == SkXfermode::kSrcOver_Mode;
        deferInfo.opaqueOverBounds = isOpaqueOverBounds(state) && mBitmap->isOpaque();
    }

private:
    const SkBitmap* mBitmap;
    const Res_png_9patch* mPatch;

    uint32_t mGenerationId;
    const Patch* mMesh;

    const AssetAtlas& mAtlas;
    uint32_t mEntryGenerationId;
    AssetAtlas::Entry* mEntry;
};

class DrawColorOp : public DrawOp {
public:
    DrawColorOp(int color, SkXfermode::Mode mode)
            : DrawOp(NULL), mColor(color), mMode(mode) {
		type = DrawColorOpT;//ligengchao 
	}

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawColor(mColor, mMode);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw color %#x, mode %d", mColor, mMode);
    }

    virtual const char* name() { return "DrawColor"; }

private:
    int mColor;
    SkXfermode::Mode mMode;
};

class DrawStrokableOp : public DrawBoundedOp {
public:
    DrawStrokableOp(float left, float top, float right, float bottom, const SkPaint* paint)
            : DrawBoundedOp(left, top, right, bottom, paint){
		type = DrawStrokableOpT;//ligengchao 
	}
    DrawStrokableOp(const Rect& localBounds, const SkPaint* paint)
            : DrawBoundedOp(localBounds, paint) {
		type = DrawStrokableOpT;//ligengchao 
	}

    virtual bool getLocalBounds(Rect& localBounds) {
        localBounds.set(mLocalBounds);
        if (mPaint && mPaint->getStyle() != SkPaint::kFill_Style) {
            localBounds.outset(strokeWidthOutset());
        }
        return true;
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        if (mPaint->getPathEffect()) {
            deferInfo.batchId = DeferredDisplayList::kOpBatch_AlphaMaskTexture;
        } else {
            deferInfo.batchId = mPaint->isAntiAlias() ?
                    DeferredDisplayList::kOpBatch_AlphaVertices :
                    DeferredDisplayList::kOpBatch_Vertices;
        }
    }
//ligengchao start
	virtual bool equal2(DisplayListOp* DisplayListOp2){
		if(this->DrawBoundedOp::equal2(DisplayListOp2))
			return true;
		return false;
	}
//ligengchao end
};

class DrawRectOp : public DrawStrokableOp {
public:
    DrawRectOp(float left, float top, float right, float bottom, const SkPaint* paint)
            : DrawStrokableOp(left, top, right, bottom, paint) {
		type = DrawRectOpT;//ligengchao 
	}

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawRect(mLocalBounds.left, mLocalBounds.top,
                mLocalBounds.right, mLocalBounds.bottom, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Rect " RECT_STRING, RECT_ARGS(mLocalBounds));
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        DrawStrokableOp::onDefer(renderer, deferInfo, state);
        deferInfo.opaqueOverBounds = isOpaqueOverBounds(state) &&
                mPaint->getStyle() == SkPaint::kFill_Style;
    }

    virtual const char* name() { return "DrawRect"; }
};

class DrawRectsOp : public DrawBoundedOp {
public:
    DrawRectsOp(const float* rects, int count, const SkPaint* paint)
            : DrawBoundedOp(rects, count, paint),
            mRects(rects), mCount(count) {
		type = DrawRectsOpT;//ligengchao 
	}
    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawRects(mRects, mCount, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Rects count %d", mCount);
    }

    virtual const char* name() { return "DrawRects"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = DeferredDisplayList::kOpBatch_Vertices;
    }

private:
    const float* mRects;
    int mCount;
};

class DrawRoundRectOp : public DrawStrokableOp {
public:
    DrawRoundRectOp(float left, float top, float right, float bottom,
            float rx, float ry, const SkPaint* paint)
            : DrawStrokableOp(left, top, right, bottom, paint), mRx(rx), mRy(ry) {
		type = DrawRoundRectOpT;//ligengchao 
	}

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawRoundRect(mLocalBounds.left, mLocalBounds.top,
                mLocalBounds.right, mLocalBounds.bottom, mRx, mRy, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw RoundRect " RECT_STRING ", rx %f, ry %f", RECT_ARGS(mLocalBounds), mRx, mRy);
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        DrawStrokableOp::onDefer(renderer, deferInfo, state);
        if (!mPaint->getPathEffect()) {
            renderer.getCaches().tessellationCache.precacheRoundRect(state.mMatrix, *mPaint,
                    mLocalBounds.getWidth(), mLocalBounds.getHeight(), mRx, mRy);
        }
    }

    virtual const char* name() { return "DrawRoundRect"; }

private:
    float mRx;
    float mRy;
};

class DrawRoundRectPropsOp : public DrawOp {
public:
    DrawRoundRectPropsOp(float* left, float* top, float* right, float* bottom,
            float *rx, float *ry, const SkPaint* paint)
            : DrawOp(paint), mLeft(left), mTop(top), mRight(right), mBottom(bottom),
            mRx(rx), mRy(ry) {
		type =DrawRoundRectPropsOpT;//ligengchao 
	}

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawRoundRect(*mLeft, *mTop, *mRight, *mBottom,
                *mRx, *mRy, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw RoundRect Props " RECT_STRING ", rx %f, ry %f",
                *mLeft, *mTop, *mRight, *mBottom, *mRx, *mRy);
    }

    virtual const char* name() { return "DrawRoundRectProps"; }

private:
    float* mLeft;
    float* mTop;
    float* mRight;
    float* mBottom;
    float* mRx;
    float* mRy;
};

class DrawCircleOp : public DrawStrokableOp {
public:
    DrawCircleOp(float x, float y, float radius, const SkPaint* paint)
            : DrawStrokableOp(x - radius, y - radius, x + radius, y + radius, paint),
            mX(x), mY(y), mRadius(radius) {
		type = DrawCircleOpT;//ligengchao 
	}
    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawCircle(mX, mY, mRadius, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Circle x %f, y %f, r %f", mX, mY, mRadius);
    }

    virtual const char* name() { return "DrawCircle"; }

private:
    float mX;
    float mY;
    float mRadius;
};

class DrawCirclePropsOp : public DrawOp {
public:
    DrawCirclePropsOp(float* x, float* y, float* radius, const SkPaint* paint)
            : DrawOp(paint), mX(x), mY(y), mRadius(radius) {
		type = DrawCirclePropsOpT;//ligengchao 
	}
    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawCircle(*mX, *mY, *mRadius, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Circle Props x %p, y %p, r %p", mX, mY, mRadius);
    }

    virtual const char* name() { return "DrawCircleProps"; }

private:
    float* mX;
    float* mY;
    float* mRadius;
};

class DrawOvalOp : public DrawStrokableOp {
public:
    DrawOvalOp(float left, float top, float right, float bottom, const SkPaint* paint)
            : DrawStrokableOp(left, top, right, bottom, paint) {
		type = DrawOvalOpT;//ligengchao 
	}

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawOval(mLocalBounds.left, mLocalBounds.top,
                mLocalBounds.right, mLocalBounds.bottom, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Oval " RECT_STRING, RECT_ARGS(mLocalBounds));
    }

    virtual const char* name() { return "DrawOval"; }
};

class DrawArcOp : public DrawStrokableOp {
public:
    DrawArcOp(float left, float top, float right, float bottom,
            float startAngle, float sweepAngle, bool useCenter, const SkPaint* paint)
            : DrawStrokableOp(left, top, right, bottom, paint),
            mStartAngle(startAngle), mSweepAngle(sweepAngle), mUseCenter(useCenter) {
		type = DrawArcOpT;//ligengchao 
	}
    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawArc(mLocalBounds.left, mLocalBounds.top,
                mLocalBounds.right, mLocalBounds.bottom,
                mStartAngle, mSweepAngle, mUseCenter, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Arc " RECT_STRING ", start %f, sweep %f, useCenter %d",
                RECT_ARGS(mLocalBounds), mStartAngle, mSweepAngle, mUseCenter);
    }

    virtual const char* name() { return "DrawArc"; }

private:
    float mStartAngle;
    float mSweepAngle;
    bool mUseCenter;
};

class DrawPathOp : public DrawBoundedOp {
public:
    DrawPathOp(const SkPath* path, const SkPaint* paint)
            : DrawBoundedOp(paint), mPath(path) {
		type = DrawPathOpT;//ligengchao 
        float left, top, offset;
        uint32_t width, height;
        PathCache::computePathBounds(path, paint, left, top, offset, width, height);
        left -= offset;
        top -= offset;
        mLocalBounds.set(left, top, left + width, top + height);
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawPath(mPath, getPaint(renderer));
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        const SkPaint* paint = getPaint(renderer);
        renderer.getCaches().pathCache.precache(mPath, paint);

        deferInfo.batchId = DeferredDisplayList::kOpBatch_AlphaMaskTexture;
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Path %p in " RECT_STRING, mPath, RECT_ARGS(mLocalBounds));
    }

    virtual const char* name() { return "DrawPath"; }

private:
    const SkPath* mPath;
};

class DrawLinesOp : public DrawBoundedOp {
public:
    DrawLinesOp(const float* points, int count, const SkPaint* paint)
            : DrawBoundedOp(points, count, paint),
            mPoints(points), mCount(count)  {
		type = DrawLinesOpT;//ligengchao 
        mLocalBounds.outset(strokeWidthOutset());
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawLines(mPoints, mCount, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Lines count %d", mCount);
    }

    virtual const char* name() { return "DrawLines"; }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        deferInfo.batchId = mPaint->isAntiAlias() ?
                DeferredDisplayList::kOpBatch_AlphaVertices :
                DeferredDisplayList::kOpBatch_Vertices;
    }

protected:
    const float* mPoints;
    int mCount;
};

class DrawPointsOp : public DrawLinesOp {
public:
    DrawPointsOp(const float* points, int count, const SkPaint* paint)
            : DrawLinesOp(points, count, paint) {
		type = DrawPointsOpT;//ligengchao 
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawPoints(mPoints, mCount, getPaint(renderer));
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Points count %d", mCount);
    }

    virtual const char* name() { return "DrawPoints"; }
};

class DrawSomeTextOp : public DrawOp {
public:
    DrawSomeTextOp(const char* text, int bytesCount, int count, const SkPaint* paint)
            : DrawOp(paint), mText(text), mBytesCount(bytesCount), mCount(count) {
		type = DrawSomeTextOpT;//ligengchao 
    }
    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw some text, %d bytes", mBytesCount);
    }

    virtual bool hasTextShadow() const {
        return OpenGLRenderer::hasTextShadow(mPaint);
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        const SkPaint* paint = getPaint(renderer);
        FontRenderer& fontRenderer = renderer.getCaches().fontRenderer->getFontRenderer(paint);
        fontRenderer.precache(paint, mText, mCount, SkMatrix::I());

        deferInfo.batchId = mPaint->getColor() == SK_ColorBLACK ?
                DeferredDisplayList::kOpBatch_Text :
                DeferredDisplayList::kOpBatch_ColorText;
    }

protected:
    const char* mText;
    int mBytesCount;
    int mCount;
};

class DrawTextOnPathOp : public DrawSomeTextOp {
public:
    DrawTextOnPathOp(const char* text, int bytesCount, int count,
            const SkPath* path, float hOffset, float vOffset, const SkPaint* paint)
            : DrawSomeTextOp(text, bytesCount, count, paint),
            mPath(path), mHOffset(hOffset), mVOffset(vOffset) {
		type = DrawTextOnPathOpT;//ligengchao 
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawTextOnPath(mText, mBytesCount, mCount, mPath,
                mHOffset, mVOffset, getPaint(renderer));
    }

    virtual const char* name() { return "DrawTextOnPath"; }

private:
    const SkPath* mPath;
    float mHOffset;
    float mVOffset;
};

class DrawPosTextOp : public DrawSomeTextOp {
public:
    DrawPosTextOp(const char* text, int bytesCount, int count,
            const float* positions, const SkPaint* paint)
            : DrawSomeTextOp(text, bytesCount, count, paint), mPositions(positions) {
		type = DrawPosTextOpT;//ligengchao 
        /* TODO: inherit from DrawBounded and init mLocalBounds */
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawPosText(mText, mBytesCount, mCount, mPositions, getPaint(renderer));
    }

    virtual const char* name() { return "DrawPosText"; }

private:
    const float* mPositions;
};

class DrawTextOp : public DrawStrokableOp {
public:
    DrawTextOp(const char* text, int bytesCount, int count, float x, float y,
            const float* positions, const SkPaint* paint, float totalAdvance, const Rect& bounds)
            : DrawStrokableOp(bounds, paint), mText(text), mBytesCount(bytesCount), mCount(count),
            mX(x), mY(y), mPositions(positions), mTotalAdvance(totalAdvance) {
		type = DrawTextOpT;//ligengchao 
        mPrecacheTransform = SkMatrix::InvalidMatrix();
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        const SkPaint* paint = getPaint(renderer);
        FontRenderer& fontRenderer = renderer.getCaches().fontRenderer->getFontRenderer(paint);
        SkMatrix transform;
        renderer.findBestFontTransform(state.mMatrix, &transform);
        if (mPrecacheTransform != transform) {
            fontRenderer.precache(paint, mText, mCount, transform);
            mPrecacheTransform = transform;
        }
        deferInfo.batchId = mPaint->getColor() == SK_ColorBLACK ?
                DeferredDisplayList::kOpBatch_Text :
                DeferredDisplayList::kOpBatch_ColorText;

        deferInfo.mergeId = reinterpret_cast<mergeid_t>(mPaint->getColor());

        // don't merge decorated text - the decorations won't draw in order
        bool hasDecorations = mPaint->getFlags()
                & (SkPaint::kUnderlineText_Flag | SkPaint::kStrikeThruText_Flag);

        deferInfo.mergeable = state.mMatrix.isPureTranslate()
                && !hasDecorations
                && OpenGLRenderer::getXfermodeDirect(mPaint) == SkXfermode::kSrcOver_Mode;
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        Rect bounds;
        getLocalBounds(bounds);
        return renderer.drawText(mText, mBytesCount, mCount, mX, mY,
                mPositions, getPaint(renderer), mTotalAdvance, bounds);
    }

    virtual status_t multiDraw(OpenGLRenderer& renderer, Rect& dirty,
            const Vector<OpStatePair>& ops, const Rect& bounds) {
        status_t status = DrawGlInfo::kStatusDone;
        for (unsigned int i = 0; i < ops.size(); i++) {
            const DeferredDisplayState& state = *(ops[i].state);
            DrawOpMode drawOpMode = (i == ops.size() - 1) ? kDrawOpMode_Flush : kDrawOpMode_Defer;
            renderer.restoreDisplayState(state, true); // restore all but the clip

            DrawTextOp& op = *((DrawTextOp*)ops[i].op);
            // quickReject() will not occure in drawText() so we can use mLocalBounds
            // directly, we do not need to account for shadow by calling getLocalBounds()
            status |= renderer.drawText(op.mText, op.mBytesCount, op.mCount, op.mX, op.mY,
                    op.mPositions, op.getPaint(renderer), op.mTotalAdvance, op.mLocalBounds,
                    drawOpMode);
        }
        return status;
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Text of count %d, bytes %d", mCount, mBytesCount);
    }

    virtual const char* name() { return "DrawText"; }

	
	//ligengchao start
	virtual bool equal(DisplayListOp* DisplayListOptmp){		
		if(this->DrawBoundedOp::equal2(DisplayListOptmp)) {
			DrawTextOp* DisplayListOp2 = (DrawTextOp*)DisplayListOptmp;
//			for(int i = 0; i < mBytesCount; i++){
//				LOGD("DrawTextOp: %c", mText[i]);
//			}
//			LOGD("DrawTextOp: %d %d %d %d %d %d %d %d",mBytesCount == DisplayListOp2->mBytesCount, mCount == DisplayListOp2->mCount, mX == DisplayListOp2->mX, 
//				mY == DisplayListOp2->mY, *mPositions == *(DisplayListOp2->mPositions), mTotalAdvance == DisplayListOp2->mTotalAdvance
//				, memcmp(mText, DisplayListOp2->mText, mBytesCount) == 0, mPrecacheTransform == DisplayListOp2->mPrecacheTransform );
			if(mBytesCount == DisplayListOp2->mBytesCount && mCount == DisplayListOp2->mCount && mX == DisplayListOp2->mX && 
				mY == DisplayListOp2->mY && *mPositions == *(DisplayListOp2->mPositions) && mTotalAdvance == DisplayListOp2->mTotalAdvance
				&& memcmp(mText, DisplayListOp2->mText, mBytesCount) == 0){//&& mPrecacheTransform == DisplayListOp2->mPrecacheTransform 
				return true;
			}
								
		}
			
		return false;
	}
	//ligengchao end

public: //ligengchao private -- public
    const char* mText;
    int mBytesCount;
    int mCount;
    float mX;
    float mY;
    const float* mPositions;
    float mTotalAdvance;
    SkMatrix mPrecacheTransform;
};

///////////////////////////////////////////////////////////////////////////////
// SPECIAL DRAW OPERATIONS
///////////////////////////////////////////////////////////////////////////////

class DrawFunctorOp : public DrawOp {
public:
    DrawFunctorOp(Functor* functor)
            : DrawOp(NULL), mFunctor(functor) {
		type = DrawFunctorOpT;//ligengchao 
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        renderer.startMark("GL functor");
        status_t ret = renderer.callDrawGLFunction(mFunctor, dirty);
        renderer.endMark();
        return ret;
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Functor %p", mFunctor);
    }

    virtual const char* name() { return "DrawFunctor"; }

private:
    Functor* mFunctor;
};

class DrawRenderNodeOp : public DrawBoundedOp {
    friend class RenderNode; // grant RenderNode access to info of child
    friend class DisplayListData; // grant DisplayListData access to info of child
public:
    DrawRenderNodeOp(RenderNode* renderNode, int flags, const mat4& transformFromParent)
            : DrawBoundedOp(0, 0, renderNode->getWidth(), renderNode->getHeight(), 0),
            mRenderNode(renderNode), mFlags(flags), mTransformFromParent(transformFromParent) {
		type = DrawRenderNodeOpT;//ligengchao 
    }

    virtual void defer(DeferStateStruct& deferStruct, int saveCount, int level,
            bool useQuickReject) {
        if (mRenderNode->isRenderable() && !mSkipInOrderDraw) {
            mRenderNode->defer(deferStruct, level + 1);
        }
    }

    virtual void replay(ReplayStateStruct& replayStruct, int saveCount, int level,
            bool useQuickReject) {
        if (mRenderNode->isRenderable() && !mSkipInOrderDraw) {
            mRenderNode->replay(replayStruct, level + 1);
        }
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        LOG_ALWAYS_FATAL("should not be called, because replay() is overridden");
        return 0;
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw RenderNode %p %s, flags %#x", mRenderNode, mRenderNode->getName(), mFlags);
        if (mRenderNode && (logFlags & kOpLogFlag_Recurse)) {
            mRenderNode->output(level + 1);
        }
    }

    virtual const char* name() { return "DrawRenderNode"; }

    RenderNode* renderNode() { return mRenderNode; }

private:
    RenderNode* mRenderNode;
    const int mFlags;

    ///////////////////////////
    // Properties below are used by RenderNode::computeOrderingImpl() and issueOperations()
    ///////////////////////////
    /**
     * Records transform vs parent, used for computing total transform without rerunning DL contents
     */
    const mat4 mTransformFromParent;

    /**
     * Holds the transformation between the projection surface ViewGroup and this RenderNode
     * drawing instance. Represents any translations / transformations done within the drawing of
     * the compositing ancestor ViewGroup's draw, before the draw of the View represented by this
     * DisplayList draw instance.
     *
     * Note: doesn't include transformation within the RenderNode, or its properties.
     */
    mat4 mTransformFromCompositingAncestor;
    bool mSkipInOrderDraw;
};

/**
 * Not a canvas operation, used only by 3d / z ordering logic in RenderNode::iterate()
 */
class DrawShadowOp : public DrawOp {
public:
    DrawShadowOp(const mat4& transformXY, const mat4& transformZ,
            float casterAlpha, const SkPath* casterOutline)
        : DrawOp(NULL)
        , mTransformXY(transformXY)
        , mTransformZ(transformZ)
        , mCasterAlpha(casterAlpha)
        , mCasterOutline(casterOutline) {
		type = DrawShadowOpT;//ligengchao 
    }

    virtual void onDefer(OpenGLRenderer& renderer, DeferInfo& deferInfo,
            const DeferredDisplayState& state) {
        renderer.getCaches().tessellationCache.precacheShadows(&state.mMatrix,
                renderer.getLocalClipBounds(), isCasterOpaque(), mCasterOutline,
                &mTransformXY, &mTransformZ, renderer.getLightCenter(), renderer.getLightRadius());
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        TessellationCache::vertexBuffer_pair_t buffers;
        Matrix4 drawTransform(*(renderer.currentTransform()));
        renderer.getCaches().tessellationCache.getShadowBuffers(&drawTransform,
                renderer.getLocalClipBounds(), isCasterOpaque(), mCasterOutline,
                &mTransformXY, &mTransformZ, renderer.getLightCenter(), renderer.getLightRadius(),
                buffers);

        return renderer.drawShadow(mCasterAlpha, buffers.first, buffers.second);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOGS("DrawShadow");
    }

    virtual const char* name() { return "DrawShadow"; }

private:
    bool isCasterOpaque() { return mCasterAlpha >= 1.0f; }

    const mat4 mTransformXY;
    const mat4 mTransformZ;
    const float mCasterAlpha;
    const SkPath* mCasterOutline;
};

class DrawLayerOp : public DrawOp {
public:
    DrawLayerOp(Layer* layer, float x, float y)
            : DrawOp(NULL), mLayer(layer), mX(x), mY(y) {
		type = DrawLayerOpT;//ligengchao 
    }

    virtual status_t applyDraw(OpenGLRenderer& renderer, Rect& dirty) {
        return renderer.drawLayer(mLayer, mX, mY);
    }

    virtual void output(int level, uint32_t logFlags) const {
        OP_LOG("Draw Layer %p at %f %f", mLayer, mX, mY);
    }

    virtual const char* name() { return "DrawLayer"; }

private:
    Layer* mLayer;
    float mX;
    float mY;
};

}; // namespace uirenderer
}; // namespace android

#endif // ANDROID_HWUI_DISPLAY_OPERATION_H
