/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/Math/Vector2.h>
#include "StandardMaterial.h"
#include "GraphicsManager.h"
#include "glactor.h"
#include <EMotionFX/Source/TransformData.h>
#include <MCore/Source/UnicodeString.h>


namespace RenderGL
{
    // constructor
    StandardMaterial::StandardMaterial(GLActor* actor)
        : Material(actor)
    {
        mMaterial           = nullptr;
        mActiveShader       = nullptr;
        mAttributesUpdated  = true;

        mDiffuseMap     = GetGraphicsManager()->GetTextureCache()->GetWhiteTexture();
        mSpecularMap    = GetGraphicsManager()->GetTextureCache()->GetWhiteTexture();
        mNormalMap      = GetGraphicsManager()->GetTextureCache()->GetDefaultNormalTexture();

        mShaders.SetMemoryCategory(MEMCATEGORY_RENDERING);

        SetAttribute(LIGHTING,  true);
        SetAttribute(SKINNING,  false);
        SetAttribute(SHADOWS,   false);
        SetAttribute(TEXTURING, true);
    }


    // destructor
    StandardMaterial::~StandardMaterial()
    {
    }


    // activate the material
    void StandardMaterial::Activate(uint32 flags)
    {
        // update shader attributes
        UpdateShader();

        // check if the shader is valid and return in case it's not
        if (mActiveShader == nullptr)
        {
            return;
        }

        // get the graphics manager shortcut
        RenderGL::GraphicsManager* gfx = GetGraphicsManager();

        if (flags & GLOBAL)
        {
            mActiveShader->Activate();

            // vertex attributes
            uint32 stride = mAttributes[SKINNING] ? sizeof(SkinnedVertex) : sizeof(StandardVertex);

            static char* structStart  = reinterpret_cast<char*>(reinterpret_cast<SkinnedVertex*>(static_cast<char*>(0)));
            static size_t offsetOfNormal = static_cast<size_t>((reinterpret_cast<char*>(&static_cast<SkinnedVertex*>(0)->mNormal)) - structStart);
            static size_t offsetOfTangent  = static_cast<size_t>((reinterpret_cast<char*>(&static_cast<SkinnedVertex*>(0)->mTangent)) - structStart);
            static size_t offsetOfUV       = static_cast<size_t>((reinterpret_cast<char*>(&static_cast<SkinnedVertex*>(0)->mUV)) - structStart);
            static size_t offsetOfWeights  = static_cast<size_t>((reinterpret_cast<char*>(&static_cast<SkinnedVertex*>(0)->mWeights)) - structStart);
            static size_t offsetOfBoneIndices = static_cast<size_t>((reinterpret_cast<char*>(&static_cast<SkinnedVertex*>(0)->mBoneIndices)) - structStart);

            mActiveShader->SetAttribute("inPosition", 3, GL_FLOAT, stride, 0);
            mActiveShader->SetAttribute("inNormal",  3, GL_FLOAT, stride, offsetOfNormal);
            mActiveShader->SetAttribute("inTangent", 4, GL_FLOAT, stride, offsetOfTangent);
            mActiveShader->SetAttribute("inUV", 2, GL_FLOAT, stride, offsetOfUV);

            // vertex weights & indices
            if (mAttributes[SKINNING])
            {
                mActiveShader->SetAttribute("inWeights", 4, GL_FLOAT, stride, offsetOfWeights);
                mActiveShader->SetAttribute("inIndices", 4, GL_FLOAT, stride, offsetOfBoneIndices);
            }

            // set the view projection matrix
            MCommon::Camera* camera = GetGraphicsManager()->GetCamera();
            mActiveShader->SetUniform("matViewProj", camera->GetViewProjMatrix());
            mActiveShader->SetUniform("matView", camera->GetViewMatrix());

            // lights
            //      if (mAttributes[LIGHTING])
            {
                AZ::Vector3 mainLightDir(0.0f, -1.0f, 0.0f);
                mainLightDir *= MCore::Matrix::RotationMatrixZ(MCore::Math::DegreesToRadians(gfx->GetMainLightAngleA())) * MCore::Matrix::RotationMatrixX(MCore::Math::DegreesToRadians(gfx->GetMainLightAngleB()));
                mainLightDir.Normalize();
                mActiveShader->SetUniform("mainLightDir", mainLightDir);
                mActiveShader->SetUniform("skyColor", mActor->GetSkyColor() * gfx->GetMainLightIntensity());
                mActiveShader->SetUniform("groundColor", mActor->GetGroundColor());
                mActiveShader->SetUniform("eyePoint", camera->GetPosition());

                AZ::Vector3 rimLightDir = camera->GetViewMatrix().GetUp();
                rimLightDir *= MCore::Matrix::RotationMatrixZ(MCore::Math::DegreesToRadians(gfx->GetRimAngle()));
                rimLightDir.Normalize();
                mActiveShader->SetUniform("rimLightDir", rimLightDir);

                mActiveShader->SetUniform("rimLightFactor", gfx->GetRimIntensity());
                mActiveShader->SetUniform("rimWidth",       gfx->GetRimWidth());
                mActiveShader->SetUniform("rimLightColor",  gfx->GetRimColor());
            }
        }

        // Local settings
        if (flags & LOCAL)
        {
            EMotionFX::StandardMaterial* stdMaterial = (mMaterial->GetType() == EMotionFX::StandardMaterial::TYPE_ID) ? static_cast<EMotionFX::StandardMaterial*>(mMaterial) : nullptr;

            if (mDiffuseMap == nullptr || mDiffuseMap == gfx->GetTextureCache()->GetWhiteTexture() && stdMaterial)
            {
                mActiveShader->SetUniform("diffuseColor", stdMaterial->GetDiffuse());
            }
            else
            {
                mActiveShader->SetUniform("diffuseColor", MCore::RGBAColor(1.0f, 1.0f, 1.0f, 1.0f));
            }

            //if (mAttributes[LIGHTING])
            {
                if (stdMaterial)
                {
                    MCore::RGBAColor specularColor = stdMaterial->GetSpecular() * (stdMaterial->GetShineStrength() * gfx->GetMainLightIntensity() * gfx->GetSpecularIntensity());
                    mActiveShader->SetUniform("specularPower", stdMaterial->GetShine());
                    mActiveShader->SetUniform("lightSpecular", specularColor);
                }
                else
                {
                    MCore::RGBAColor specularColor = MCore::RGBAColor(1.0f, 1.0f, 1.0f) * (1.0f * gfx->GetMainLightIntensity() * gfx->GetSpecularIntensity());
                    mActiveShader->SetUniform("specularPower", 25.0f);
                    mActiveShader->SetUniform("lightSpecular", specularColor);
                }

                mActiveShader->SetUniform("normalMap", mNormalMap);
            }

            //if (mAttributes[TEXTURING])
            {
                mActiveShader->SetUniform("diffuseMap", mDiffuseMap);
                mActiveShader->SetUniform("specularMap", mSpecularMap);
            }
        }


        // update the advanced rendering settings
        mActiveShader->SetUniform("glowThreshold",      gfx->GetBloomThreshold());
        mActiveShader->SetUniform("focalPlaneDepth",    gfx->GetDOFFocalDistance());
        mActiveShader->SetUniform("nearPlaneDepth",     gfx->GetDOFNear());
        mActiveShader->SetUniform("farPlaneDepth",      gfx->GetDOFFar());
        mActiveShader->SetUniform("blurCutoff",         1.0f);
    }


    // deactivate the standard material
    void StandardMaterial::Deactivate()
    {
        // check if the shader is valid and return in case it's not
        if (mActiveShader == nullptr)
        {
            return;
        }

        // deactivate the active shader
        mActiveShader->Deactivate();
    }


    // initialize the standard material
    bool StandardMaterial::Init(EMotionFX::Material* material)
    {
        mMaterial = material;

        if (material->GetType() == EMotionFX::StandardMaterial::TYPE_ID)
        {
            EMotionFX::StandardMaterial* stdMaterial = static_cast<EMotionFX::StandardMaterial*>(material);

            // get the number of material layers and iterate through them
            const uint32 numLayers = stdMaterial->GetNumLayers();
            for (uint32 i = 0; i < numLayers; ++i)
            {
                EMotionFX::StandardMaterialLayer* layer = stdMaterial->GetLayer(i);
                switch (layer->GetType())
                {
                case EMotionFX::StandardMaterialLayer::LAYERTYPE_DIFFUSE:
                {
                    mDiffuseMap   = LoadTexture(layer->GetFileName());
                    if (mDiffuseMap  == nullptr)
                    {
                        mDiffuseMap = GetGraphicsManager()->GetTextureCache()->GetWhiteTexture();
                    }
                } break;
                case EMotionFX::StandardMaterialLayer::LAYERTYPE_SHINESTRENGTH:
                {
                    mSpecularMap  = LoadTexture(layer->GetFileName());
                    if (mSpecularMap == nullptr)
                    {
                        mSpecularMap = GetGraphicsManager()->GetTextureCache()->GetWhiteTexture();
                    }
                } break;
                case EMotionFX::StandardMaterialLayer::LAYERTYPE_BUMP:
                {
                    mNormalMap    = LoadTexture(layer->GetFileName());
                    if (mNormalMap   == nullptr)
                    {
                        mNormalMap = GetGraphicsManager()->GetTextureCache()->GetDefaultNormalTexture();
                    }
                } break;
                case EMotionFX::StandardMaterialLayer::LAYERTYPE_NORMALMAP:
                {
                    mNormalMap    = LoadTexture(layer->GetFileName());
                    if (mNormalMap   == nullptr)
                    {
                        mNormalMap = GetGraphicsManager()->GetTextureCache()->GetDefaultNormalTexture();
                    }
                } break;
                }
            }
        }

        return true;
    }


    //
    void StandardMaterial::SetAttribute(EAttribute attribute, bool enabled)
    {
        const uint32 index = (uint32)attribute;

        if (mAttributes[index] != enabled)
        {
            mAttributes[index] = enabled;
            mAttributesUpdated = true;
        }
    }


    // render the actor instance using the standard material
    void StandardMaterial::Render(EMotionFX::ActorInstance* actorInstance, const Primitive* primitive)
    {
        // check if the shader is valid and return in case it's not
        if (mActiveShader == nullptr)
        {
            return;
        }

        // change depth buffer settings
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        EMotionFX::TransformData* transformData = actorInstance->GetTransformData();

        // global transforms
        if (mAttributes[SKINNING])
        {
            MCore::Matrix* globalMatrices       = transformData->GetGlobalInclusiveMatrices();
            MCore::Matrix* invBindPoseMatrices  = actorInstance->GetActor()->GetInverseBindPoseGlobalMatrices().GetPtr();

            MCore::Matrix invNodeTM = globalMatrices[primitive->mNodeIndex];
            invNodeTM.Inverse();

            // multiple each transform by its inverse bind pose
            const uint32 numBones = primitive->mBoneNodeIndices.GetLength();
            for (uint32 i = 0; i < numBones; ++i)
            {
                const uint32 nodeNr = primitive->mBoneNodeIndices[i];
                mBoneMatrices[i] = invBindPoseMatrices[nodeNr];
                mBoneMatrices[i].MultMatrix4x3(globalMatrices[nodeNr]);
                mBoneMatrices[i].MultMatrix4x3(invNodeTM);
            }

            mActiveShader->SetUniform("matBones", mBoneMatrices, numBones);
        }

        MCommon::Camera*    camera          = GetGraphicsManager()->GetCamera();
        MCore::Matrix       global          = transformData->GetGlobalInclusiveMatrix(primitive->mNodeIndex);
        MCore::Matrix       globalView      = transformData->GetGlobalInclusiveMatrix(primitive->mNodeIndex) * camera->GetViewMatrix();
        MCore::Matrix       globalViewProj  = global * camera->GetViewProjMatrix();
        MCore::Matrix       globalIT        = global;
        globalIT.Inverse();
        globalIT.Transpose();

        mActiveShader->SetUniform("matWorld", global);
        mActiveShader->SetUniform("matWorldIT", globalIT);
        mActiveShader->SetUniform("matWorldView", globalView);
        mActiveShader->SetUniform("matWorldViewProj", globalViewProj);

        // render the primitive
        glDrawElements(GL_TRIANGLES, primitive->mNumTriangles * 3, GL_UNSIGNED_INT, (GLvoid*)(primitive->mIndexOffset * sizeof(uint32)));
    }


    // update the shader
    void StandardMaterial::UpdateShader()
    {
        // check if any attibutes have changed and skip directly if not
        if (mAttributesUpdated == false)
        {
            return;
        }

        // reset the active shader
        mActiveShader = nullptr;

        // get the number of shaders and iterate through them
        const uint32 numShaders = mShaders.GetLength();
        for (uint32 i = 0; i < numShaders; ++i)
        {
            if (mShaders[i] == nullptr)
            {
                continue;
            }

            // check the shader for each define
            bool match = true;
            for (uint32 n = 0; n < NUM_ATTRIBUTES; ++n)
            {
                if (mAttributes[n])
                {
                    if (mShaders[i]->CheckIfIsDefined(AttributeToString((EAttribute)n)) == false)
                    {
                        match = false;
                        break;
                    }
                }
                else
                {
                    if (mShaders[i]->CheckIfIsDefined(AttributeToString((EAttribute)n)))
                    {
                        match = false;
                        break;
                    }
                }
            }

            // in case we have found a matching shader update the active shader
            if (match)
            {
                mActiveShader = mShaders[i];
                break;
            }
        }

        // if we didn't find a matching shader, compile it new
        if (mActiveShader == nullptr)
        {
            // if this function gets called at runtime something is wrong, go bug hunting!

            // construct an array of string attributes
            MCore::Array<MCore::String> defines;
            for (uint32 n = 0; n < NUM_ATTRIBUTES; ++n)
            {
                if (mAttributes[n])
                {
                    defines.Add(AttributeToString((EAttribute)n));
                }
            }

            // compile shader and add it to the list of shaders
            mActiveShader = GetGraphicsManager()->LoadShader("StandardMaterial_VS.glsl", "StandardMaterial_PS.glsl", defines);
            mShaders.Add(mActiveShader);
        }

        mAttributesUpdated = false;
    }
}
