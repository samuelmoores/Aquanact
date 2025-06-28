#include "Animation.h"

Animation::Animation(const char* file, Mesh& mesh)
{
    m_mesh = mesh;
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

aiNode* FindSkeletonRoot(aiNode* node, const std::unordered_set<std::string>& boneNames)
{
    if (boneNames.count(node->mName.C_Str()) > 0)
    {
        if (node->mParent == nullptr || boneNames.count(node->mParent->mName.C_Str()) == 0)
        {
            return node; // This is a root bone (has no parent that's also a bone)
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        aiNode* result = FindSkeletonRoot(node->mChildren[i], boneNames);
        if (result)
            return result;
    }

    return nullptr;
}


void Animation::assimpLoad(const std::string& path, bool flipUvs) {
    int flags = (aiPostProcessSteps)aiProcessPreset_TargetRealtime_MaxQuality;
    if (flipUvs) {
        flags |= aiProcess_FlipUVs;
    }

    const aiScene* scene = m_importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |  // <-- This one is needed!
        aiProcess_FlipUVs);

    // If the import failed, report it
    if (nullptr == scene) {
        std::cout << "ASSIMP ERROR: " << m_importer.GetErrorString() << std::endl;
        exit(1);
    }

    aiMesh* mesh = scene->mMeshes[0];

    // Step 1: Collect all bone names from the mesh
    m_numBones = 0;
    std::unordered_set<std::string> boneNames;
    for (unsigned int i = 0; i < mesh->mNumBones; ++i)\
    {
        boneNames.insert(mesh->mBones[i]->mName.C_Str());
        m_numBones++;
    }

    
    // Step 2: Traverse the scene graph to find the highest bone node
    m_root = FindSkeletonRoot(scene->mRootNode, boneNames);

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

float Animation::duration()
{
    return m_duration;
}

float Animation::ticksPerSecond()
{
    return m_ticksPerSecond;
}

const std::map<std::string, BoneAnimChannel>& Animation::boneAnimChannels() const
{
    return m_boneAnimChannels;
}

const Mesh& Animation::mesh() const
{
    return m_mesh;
}

const aiNode* Animation::root()
{
    return m_root;
}

int Animation::numBones()
{
    return m_numBones;
}
