#include "Animation.h"

Animation::Animation(const char* file)
{
    assimpLoad(file, true);
}

std::map<std::string, BoneAnimChannel> ExtractAnimationChannels(const aiAnimation* animation)
{
    std::map<std::string, BoneAnimChannel> boneChannels;

    for (unsigned int i = 0; i < animation->mNumChannels; ++i)
    {
        aiNodeAnim* channel = animation->mChannels[i];
        BoneAnimChannel boneChannel;
        boneChannel.boneName = channel->mNodeName.C_Str();

        // Positions
        for (unsigned int j = 0; j < channel->mNumPositionKeys; ++j)
        {
            KeyPosition kp;
            kp.timestamp = static_cast<float>(channel->mPositionKeys[j].mTime);
            aiVector3D pos = channel->mPositionKeys[j].mValue;
            kp.position = glm::vec3(pos.x, pos.y, pos.z);
            boneChannel.positions.push_back(kp);
        }

        // Rotations
        for (unsigned int j = 0; j < channel->mNumRotationKeys; ++j)
        {
            KeyRotation kr;
            kr.timestamp = static_cast<float>(channel->mRotationKeys[j].mTime);
            aiQuaternion rot = channel->mRotationKeys[j].mValue;
            kr.rotation = glm::quat(rot.w, rot.x, rot.y, rot.z); // glm is wxyz
            boneChannel.rotations.push_back(kr);
        }

        // Scales
        for (unsigned int j = 0; j < channel->mNumScalingKeys; ++j)
        {
            KeyScale ks;
            ks.timestamp = static_cast<float>(channel->mScalingKeys[j].mTime);
            aiVector3D scale = channel->mScalingKeys[j].mValue;
            ks.scale = glm::vec3(scale.x, scale.y, scale.z);
            boneChannel.scales.push_back(ks);
        }

        boneChannels[boneChannel.boneName] = boneChannel;
    }

    return boneChannels;
}

void Animation::assimpLoad(const std::string& path, bool flipUvs) {
    int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
    if (flipUvs) {
        flags |= aiProcess_FlipUVs;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |  // <-- This one is needed!
        aiProcess_FlipUVs);

    // If the import failed, report it
    if (nullptr == scene) {
        std::cout << "ASSIMP ERROR: " << importer.GetErrorString() << std::endl;
        exit(1);
    }

    if (scene->HasAnimations())
    {
        aiAnimation* animation = scene->mAnimations[0]; // Just using the first animation for now

        float duration = static_cast<float>(animation->mDuration);
        float ticksPerSecond = static_cast<float>(animation->mTicksPerSecond ? animation->mTicksPerSecond : 25.0f);

        std::map<std::string, BoneAnimChannel> boneAnimChannels = ExtractAnimationChannels(animation);

        m_boneAnimChannels = boneAnimChannels;
        m_duration = duration;
        m_ticksPerSecond = ticksPerSecond;
    }
    else
    {
        std::cout << "Error: no animation in file\n";
    }
}