#include "Animator.h"

Animator::Animator(Animation* animation) : m_currentTime(0.0f)
{
    m_animation = animation;
    m_root = m_animation->root();

    for (int i = 0; i < animation->numBones(); i++)
    {
        m_finalBoneMatrices.push_back(glm::mat4(0));
    }
}

void Animator::Update(float dt)
{
	m_currentTime += dt * m_animation->ticksPerSecond();
	m_currentTime = fmod(m_currentTime, m_animation->duration());
    CalculateBoneTransform(m_root, glm::mat4(1));
}

glm::mat4 AssimpToGlm(const aiMatrix4x4& mat)
{
    glm::mat4 result;
    result[0] = glm::vec4(mat.a1, mat.b1, mat.c1, mat.d1);
    result[1] = glm::vec4(mat.a2, mat.b2, mat.c2, mat.d2);
    result[2] = glm::vec4(mat.a3, mat.b3, mat.c3, mat.d3);
    result[3] = glm::vec4(mat.a4, mat.b4, mat.c4, mat.d4);
    return result;
}

glm::vec3 InterpolatePosition(const BoneAnimChannel& channel, float time)
{
    if (channel.positions.size() == 1)
        return channel.positions[0].position;

    for (size_t i = 0; i < channel.positions.size() - 1; ++i)
    {
        if (time < channel.positions[i + 1].timestamp)
        {
            const auto& start = channel.positions[i];
            const auto& end = channel.positions[i + 1];
            float delta = end.timestamp - start.timestamp;
            float factor = (time - start.timestamp) / delta;
            return glm::mix(start.position, end.position, factor);
        }
    }

    return channel.positions.back().position; // fallback
}

glm::vec3 InterpolateScale(const BoneAnimChannel& channel, float time)
{
    if (channel.scales.size() == 1)
        return channel.scales[0].scale;

    for (size_t i = 0; i < channel.scales.size() - 1; ++i)
    {
        if (time < channel.scales[i + 1].timestamp)
        {
            const auto& start = channel.scales[i];
            const auto& end = channel.scales[i + 1];
            float delta = end.timestamp - start.timestamp;
            float factor = (time - start.timestamp) / delta;
            return glm::mix(start.scale, end.scale, factor);
        }
    }

    return channel.scales.back().scale;
}

glm::quat InterpolateRotation(const BoneAnimChannel& channel, float time)
{
    if (channel.rotations.size() == 1)
        return channel.rotations[0].rotation;

    for (size_t i = 0; i < channel.rotations.size() - 1; ++i)
    {
        if (time < channel.rotations[i + 1].timestamp)
        {
            const auto& start = channel.rotations[i];
            const auto& end = channel.rotations[i + 1];
            float delta = end.timestamp - start.timestamp;
            float factor = (time - start.timestamp) / delta;
            return glm::slerp(start.rotation, end.rotation, factor);
        }
    }

    return channel.rotations.back().rotation;
}

void Animator::CalculateBoneTransform(const aiNode* node, const glm::mat4& parentTransform)
{
    std::string nodeName = node->mName.C_Str();
    glm::mat4 nodeTransform = AssimpToGlm(node->mTransformation); // base transform from the node

    // Override with animation transform if it exists
    auto channelIt = m_animation->boneAnimChannels().find(nodeName);

    if (channelIt != m_animation->boneAnimChannels().end())
    {
        const BoneAnimChannel& channel = channelIt->second;

        glm::vec3 position = InterpolatePosition(channel, m_currentTime);
        glm::quat rotation = InterpolateRotation(channel, m_currentTime);
        glm::vec3 scale = InterpolateScale(channel, m_currentTime);

        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rot = glm::toMat4(rotation);
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale);

        nodeTransform = translation * rot * scaling;
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    Skeleton skeleton = m_animation->mesh().GetSkeleton();

    int boneIndex = skeleton.boneMapping[nodeName];
    glm::mat4 offset = AssimpToGlm(m_animation->mesh().GetSkeleton().boneOffsetMatrices[boneIndex]); // from mesh
    
    m_finalBoneMatrices[boneIndex] = globalTransform * offset;

    // Recursively process all children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        CalculateBoneTransform(node->mChildren[i], globalTransform);
    }
}