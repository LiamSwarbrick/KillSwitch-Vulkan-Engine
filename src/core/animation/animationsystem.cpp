#include "animation.h"
#include "components.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

// ALL UNDER THE ASSUMPTION OF ONE SKIN PER ASSET
void Animation_Update(ECS* ecs, float dt)
{
    auto view = ecs->GetView<C_AnimatedMesh>();
    // iterate over entities using for each
    view.ForEach([&](EntityID e, C_AnimatedMesh& animatedMesh)
    {
        Asset* asset = animatedMesh.asset;

        if (!ecs->IsEntityValid(e) || !ecs->Has<C_AnimatedMesh>(e))
            return;

        // Skip if no animation is playing, asset is missing or animation is invalid
         if (!animatedMesh.isPlaying || !asset || animatedMesh.currentAnimation < 0)
             return;

        Animation& animation = asset->animations[animatedMesh.currentAnimation];

		// Update the animation time
        animatedMesh.animationTime += dt;
        if (animatedMesh.isLooping && GetDuration(animatedMesh) > 0.0f)
            animatedMesh.animationTime = fmod(animatedMesh.animationTime, GetDuration(animatedMesh));
		else if (animatedMesh.animationTime > GetDuration(animatedMesh))
			Stop(animatedMesh);

		// Vectors to store the interpolated transformations for each channel
        // PUT THESE IN THE COMPONENT WHEN BLENDING
		int animatedBoneCount = animatedMesh.joint_count;
		std::vector<glm::vec3> translations(animatedBoneCount);
		std::vector<glm::quat> rotations(animatedBoneCount);
        std::vector<glm::vec3> scales(animatedBoneCount);
        
        // Fill with default values so should just not animate if broken
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            translations[i] = glm::vec3(asset->skins[0].bones[i].translation[0], asset->skins[0].bones[i].translation[1], asset->skins[0].bones[i].translation[2]); 
			rotations[i] = glm::quat(asset->skins[0].bones[i].rotation[3], asset->skins[0].bones[i].rotation[0], asset->skins[0].bones[i].rotation[1], asset->skins[0].bones[i].rotation[2]);
			scales[i] = glm::vec3(1.0f);
		}

        // Loop through each channel and store the interpolated transform data for this frame in the vectors
        for (size_t i = 0; i < animation.channel_count; ++i)
        {
            AnimationChannel& channel = animation.channels[i];
            AnimationSampler& sampler = animation.samplers[channel.sampler_index];

            // Find the keyframes and interpolation value from the current animation time
            int firstKeyframe = 0;
            int secondKeyframe = 0;
            float interpolationFactor = 0.0f;
            for (size_t j = 0; j < sampler.input_count - 1; j++)
            {
                if (animatedMesh.animationTime < sampler.inputs[j + 1])
                {
                    firstKeyframe = j;
                    secondKeyframe = j + 1;
                    break;
                }
            }
            interpolationFactor = (animatedMesh.animationTime - sampler.inputs[firstKeyframe]) / (sampler.inputs[secondKeyframe] - sampler.inputs[firstKeyframe]);

			// Interpolate the transformations to get local matrices for each joint, 0 = translation, 1 = rotation, 2 = scale
			int jointIndex = find_bone_index(&asset->skins[0], channel.target_node_index);
            if (jointIndex == -1) continue; // Channel is useless, not affecting a joint
            if (channel.target_path == 0)
            {
                glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
                glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
                translations[jointIndex] = glm::mix(start, end, interpolationFactor);
            }
            else if (channel.target_path == 1)
            {
                glm::quat start = glm::quat(sampler.outputs[firstKeyframe * 4 + 3], sampler.outputs[firstKeyframe * 4], sampler.outputs[firstKeyframe * 4 + 1], sampler.outputs[firstKeyframe * 4 + 2]);
                glm::quat end = glm::quat(sampler.outputs[secondKeyframe * 4 + 3], sampler.outputs[secondKeyframe * 4], sampler.outputs[secondKeyframe * 4 + 1], sampler.outputs[secondKeyframe * 4 + 2]);
                rotations[jointIndex] = glm::slerp(start, end, interpolationFactor);
            }
            else if (channel.target_path == 2)
            {
                glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
                glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
                scales[jointIndex] = glm::mix(start, end, interpolationFactor);
            }
        }

        // Create vector of local joint matrices
		std::vector<glm::mat4> localJointMatrices(animatedBoneCount);
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translations[i]);
            glm::mat4 rotationMatrix = glm::mat4_cast(rotations[i]);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scales[i]);
            localJointMatrices[i] = translationMatrix * rotationMatrix * scaleMatrix;
		}

        // Initialise world joint matrices with identity matrices
		std::vector<glm::mat4> worldJointMatrices(animatedBoneCount, glm::mat4(1.0f));
        int rootBone = find_bone_index(&asset->skins[0], asset->skins[0].skeleton_root_node_index);
		CalculateWorldMatrices(asset, rootBone, glm::mat4(1.0f), localJointMatrices, worldJointMatrices);

		// Make sure joint_matrices is allocated
        if (!animatedMesh.joint_matrices)
			animatedMesh.joint_matrices = new glm::mat4[animatedBoneCount];

		// Calculate final joint matrices by multiplying world joint matrices with inverse bind matrices
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            glm::mat4 inverseBindMatrix = glm::make_mat4(asset->skins[0].inverse_bind_matrices + i * 16);
            animatedMesh.joint_matrices[i] = worldJointMatrices[i] * inverseBindMatrix;
        }
    });
}

void Start(C_AnimatedMesh& animatedMesh, const char* name) // by name
{
    if (!animatedMesh.asset)
		return;

	// Find the animation with the given name and start it
    for (size_t i = 0; i < animatedMesh.asset->animation_count; ++i)
    {
        if (strcmp(animatedMesh.asset->animations[i].name, name) == 0)
        {
            animatedMesh.currentAnimation = i;
            animatedMesh.animationTime = 0.0f;
            animatedMesh.isPlaying = true;
            return;
        }
	}   
}

void Start(C_AnimatedMesh& animatedMesh, int id) // by index
{ 
	// Check asset and animation index are valid, then start the animation
    if (!animatedMesh.asset || id < 0 || id >= animatedMesh.asset->animation_count)
        return;

	animatedMesh.currentAnimation = id;
	animatedMesh.animationTime = 0.0f;
	animatedMesh.isPlaying = true; 
}

void Stop(C_AnimatedMesh& animatedMesh) 
{ 
	animatedMesh.isPlaying = false;
}



// settings
void SetLooping(C_AnimatedMesh& animatedMesh, bool looping) 
{ 
    animatedMesh.isLooping = looping; 
} 



// state checks
bool IsRunning(const C_AnimatedMesh& animatedMesh) 
{ 
    if (!animatedMesh.isPlaying || animatedMesh.animationTime > GetDuration(animatedMesh))
		return false;
    return true; 
}



// getters
float GetDuration(const C_AnimatedMesh& animatedMesh)
{
    // Making sure an existing animation is playing
    if (animatedMesh.currentAnimation < 0 || animatedMesh.currentAnimation >= animatedMesh.asset->animation_count)
        return 0.0f;

    Animation& animation = animatedMesh.asset->animations[animatedMesh.currentAnimation];
    float duration = 0.0f;

    // Loop through all samplers to find the max timestamp, aka the duration
    for (size_t i = 0; i < animation.sampler_count; ++i)
    {
        if (animation.samplers[i].inputs[animation.samplers[i].input_count - 1] > duration)
			duration = animation.samplers[i].inputs[animation.samplers[i].input_count - 1];
    }

    return duration;
} // Get last keyframe of the current animation and return the timestamp



// calculations
int find_bone_index(Skin* skin, int target_node_index) {
    for (size_t i = 0; i < skin->joint_count; ++i) {
        if (skin->joint_node_indices[i] == target_node_index) {
            return (int)i;
        }
    }
    return -1;
}

void CalculateWorldMatrices(Asset* asset, int boneIndex, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices)
{
    // Calculate world matrix from local matrix and parent world matrix
	worldJointMatrices[boneIndex] = parentMatrix * localJointMatrices[boneIndex];

    // Recursively call for all children
    for (size_t i = 0; i < asset->skins[0].bones[boneIndex].child_count; ++i)
		CalculateWorldMatrices(asset, asset->skins[0].bones[boneIndex].children_indices[i], worldJointMatrices[boneIndex], localJointMatrices, worldJointMatrices);
}

