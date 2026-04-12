#include "animation.h"
#include "components.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

// ALL UNDER THE ASSUMPTION OF ONE SKIN PER ASSET
void Animation_Update(AdvEng::ECS* ecs, float dt)
{
    auto view = ecs->GetView<C_AnimatedMesh>();
    // iterate over entities using for each
    view.ForEach([&](AdvEng::EntityID e, C_AnimatedMesh& animatedMesh)
    {
        Asset* asset = animatedMesh.asset;

        if (!ecs->IsEntityValid(e) || !ecs->Has<C_AnimatedMesh>(e))
            return;

        // Skip if no animation is playing, asset is missing or animation is invalid
         if (!animatedMesh.isPlaying || !asset || animatedMesh.lowerBodyLayer.currentAnimation < 0 || animatedMesh.lowerBodyLayer.currentAnimation >= asset->animation_count)
             return;

        Animation& animation = asset->animations[animatedMesh.lowerBodyLayer.currentAnimation];


		// Update the lower body animation time
		UpdateLayerTime(animatedMesh, animatedMesh.lowerBodyLayer, dt, animatedMesh.playbackSpeed);
        if (animatedMesh.lowerBodyLayer.currentAnimationTime >= GetAnimationDuration(animatedMesh, animatedMesh.lowerBodyLayer.currentAnimation))
			PlayAnim(animatedMesh, "Idle", 0.2f); // SETTING TO IDLE IF ANIMATION ENDS

        // Update the upper body animation time if active
        if (animatedMesh.isUpperlayerActive)
        {
            UpdateLayerTime(animatedMesh, animatedMesh.upperBodyLayer, dt, animatedMesh.playbackSpeed);

            // If upper body animation has finished, blend back to lower body
            if (animatedMesh.upperBodyLayer.currentAnimationTime >= GetAnimationDuration(animatedMesh, animatedMesh.upperBodyLayer.currentAnimation))
                StopUpperBodyAnim(animatedMesh, 0.2f);
        }
			

        // Update the bone mask weight
		animatedMesh.upperBodyLayerWeight += (dt * animatedMesh.layerBlendDirection * animatedMesh.playbackSpeed) / animatedMesh.layerBlendDuration;
        if (animatedMesh.upperBodyLayerWeight >= 1.0f)
        {
            animatedMesh.upperBodyLayerWeight = 1.0f;
            animatedMesh.layerBlendDirection = 0;
        }
        else if (animatedMesh.upperBodyLayerWeight <= 0.0f)
        {
            animatedMesh.upperBodyLayerWeight = 0.0f;
            animatedMesh.isUpperlayerActive = false;
            animatedMesh.layerBlendDirection = 0;
		}


		// Initialising vectors of transforms for the poses
		int animatedBoneCount = animatedMesh.joint_count;
		std::vector<BoneTransform> lowerBodyPose(animatedBoneCount);
		std::vector<BoneTransform> finalPose(animatedBoneCount);
        
        // Fill with default values so should just not animate if broken, then interpolate pose
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            lowerBodyPose[i].translation = glm::vec3(asset->skins[0].bones[i].translation[0], asset->skins[0].bones[i].translation[1], asset->skins[0].bones[i].translation[2]);
            lowerBodyPose[i].rotation = glm::quat(asset->skins[0].bones[i].rotation[3], asset->skins[0].bones[i].rotation[0], asset->skins[0].bones[i].rotation[1], asset->skins[0].bones[i].rotation[2]);
            lowerBodyPose[i].scale = glm::vec3(1.0f);
		}

        // Get the pose for the animation currently active for the lower body layer
		CalculateLayerPose(asset, animatedMesh.lowerBodyLayer, lowerBodyPose);

        if (animatedMesh.isUpperlayerActive)
        {
            // Get the pose for the animation currently active for the upper body layer
			std::vector<BoneTransform> upperBodyPose = lowerBodyPose; // Just for correct size and some default values
			CalculateLayerPose(asset, animatedMesh.upperBodyLayer, upperBodyPose);

			// Overlay the upper body pose on top of the lower body pose using the bone mask and weight
            for (int i = 0; i < animatedBoneCount; ++i)
            {
                float maskValue = animatedMesh.boneMask[i] * animatedMesh.upperBodyLayerWeight;
                if (maskValue > 0.0f)
                {
                    finalPose[i].translation = glm::mix(lowerBodyPose[i].translation, upperBodyPose[i].translation, maskValue);
                    finalPose[i].rotation = glm::slerp(lowerBodyPose[i].rotation, upperBodyPose[i].rotation, maskValue);
                    finalPose[i].scale = glm::mix(lowerBodyPose[i].scale, upperBodyPose[i].scale, maskValue);
                }
            }
        }
        else
			finalPose = lowerBodyPose;


        // Create vector of local joint matrices
		std::vector<glm::mat4> localJointMatrices(animatedBoneCount);
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), finalPose[i].translation);
            glm::mat4 rotationMatrix = glm::mat4_cast(finalPose[i].rotation);
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), finalPose[i].scale);
            localJointMatrices[i] = translationMatrix * rotationMatrix * scaleMatrix;
		}


        // Initialise world joint matrices with identity matrices
		std::vector<glm::mat4> worldJointMatrices(animatedBoneCount, glm::mat4(1.0f));
        int rootBone = find_bone_index(&asset->skins[0], asset->skins[0].skeleton_root_node_index);
        auto& transform = ecs->GetComponent<C_Transform>(e);
		CalculateWorldMatrices(asset, rootBone, transform.matrix, localJointMatrices, worldJointMatrices);

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



// animation control
void OnStartAnim(C_AnimatedMesh& animatedMesh, const char* animationName)
{
	// Check asset and animation index are valid, then start the animation on the lower body layer with no blending
    if (!animatedMesh.asset)
        return;

	int animationId = GetAnimationIdFromName(animatedMesh, animationName);
    if (animationId == -1)
		return;

    animatedMesh.lowerBodyLayer.currentAnimation = animationId;
    animatedMesh.lowerBodyLayer.currentAnimationTime = 0.0f;
    animatedMesh.lowerBodyLayer.isCurrentLooping = true;
    animatedMesh.isPlaying = true;
	animatedMesh.playbackSpeed = 1.0f;
}

void PlayAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration)
{
    // Blends from the current lower body animation to the given one
    int animationId = GetAnimationIdFromName(animatedMesh, animationName);
	if (animationId == -1)
        return;

    animatedMesh.lowerBodyLayer.isBlending = true;
	animatedMesh.lowerBodyLayer.previousAnimation = animatedMesh.lowerBodyLayer.currentAnimation;
    animatedMesh.lowerBodyLayer.previousAnimationTime = animatedMesh.lowerBodyLayer.currentAnimationTime;
    animatedMesh.lowerBodyLayer.currentAnimation = animationId;
    animatedMesh.lowerBodyLayer.currentAnimationTime = 0.0f;
    animatedMesh.lowerBodyLayer.isPreviousLooping = animatedMesh.lowerBodyLayer.isCurrentLooping;
    animatedMesh.lowerBodyLayer.blendDuration = blendDuration;
	animatedMesh.lowerBodyLayer.blendTime = 0.0f;
}

void PlayUpperBodyAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration)
{
    // Blends from either lower body or current upper body to the given one
    int animationId = GetAnimationIdFromName(animatedMesh, animationName);
    if (animationId == -1)
        return;

    // Creates bone mask if one doesnt currently exist
    if (animatedMesh.boneMask.empty())
        CreateUpperBodyLayer(animatedMesh, "Spine"); // ASSUMES "Spine" IS SPLIT JOINT

    // Check if upper body is playing, if not blend from lower body
    if (!animatedMesh.isUpperlayerActive)
    {
        animatedMesh.isUpperlayerActive = true;
        animatedMesh.upperBodyLayer.currentAnimation = animationId;
        animatedMesh.upperBodyLayer.currentAnimationTime = 0.0f;
        animatedMesh.layerBlendDirection = 1;
        animatedMesh.layerBlendDuration = blendDuration;
    }
	// If upper body is already active, blend from current upper body to the new animation
    else
    {
        animatedMesh.upperBodyLayer.isBlending = true;
        animatedMesh.upperBodyLayer.previousAnimation = animatedMesh.upperBodyLayer.currentAnimation;
        animatedMesh.upperBodyLayer.previousAnimationTime = animatedMesh.upperBodyLayer.currentAnimationTime;
        animatedMesh.upperBodyLayer.currentAnimation = animationId;
        animatedMesh.upperBodyLayer.currentAnimationTime = 0.0f;
        animatedMesh.upperBodyLayer.isPreviousLooping = animatedMesh.upperBodyLayer.isCurrentLooping;
        animatedMesh.upperBodyLayer.blendDuration = blendDuration;
        animatedMesh.upperBodyLayer.blendTime = 0.0f;
    }
}

void StopUpperBodyAnim(C_AnimatedMesh& animatedMesh, float blendDuration)
{
    // Blends from current upper body to lower body
    if (!animatedMesh.isUpperlayerActive)
        return;

	animatedMesh.layerBlendDirection = -1;
	animatedMesh.layerBlendDuration = blendDuration;
}

void PlayFullBodyAnim(C_AnimatedMesh& animatedMesh, const char* animationName, float blendDuration)
{
    // Stops the current upper body animation and blends from lower body to the given one
    int animationId = GetAnimationIdFromName(animatedMesh, animationName);
    if (animationId == -1)
        return;
    
	StopUpperBodyAnim(animatedMesh, blendDuration);
	PlayAnim(animatedMesh, animationName, blendDuration);
}



// settings
void SetLooping(C_AnimatedMesh& animatedMesh, AnimationLayer& layer, bool looping)
{ 
    layer.isCurrentLooping = looping; 
} 



// state checks
bool IsRunning(const C_AnimatedMesh& animatedMesh, const AnimationLayer& layer) 
{ 
	if (layer.currentAnimation < 0 || layer.currentAnimation >= animatedMesh.asset->animation_count)
        return false;
	return true;
}



// getters
float GetAnimationDuration(const C_AnimatedMesh& animatedMesh, int animationIndex)
{
    // Check if the animation is valid
    if (animationIndex < 0 || animationIndex >= animatedMesh.asset->animation_count)
        return 0.0f;
    Animation& animation = animatedMesh.asset->animations[animationIndex];

    // Loop through all samplers to find the max timestamp, aka the duration
    float duration = 0.0f;
    for (size_t i = 0; i < animation.sampler_count; ++i)
    {
        if (animation.samplers[i].inputs[animation.samplers[i].input_count - 1] > duration)
			duration = animation.samplers[i].inputs[animation.samplers[i].input_count - 1];
    }

    return duration;
} // Get last keyframe of the current animation and return the timestamp

int GetAnimationIdFromName(const C_AnimatedMesh& animatedMesh, const char* animationName)
{
    // Find the id of an animation, return -1 if not found
    for (int i = 0; i < animatedMesh.asset->animation_count; ++i)
    {
        if (strcmp(animatedMesh.asset->animations[i].name, animationName) == 0)
            return i;
	}
    return -1;
}



// calculations
int find_bone_index(Skin* skin, int target_node_index) {
    for (size_t i = 0; i < skin->joint_count; ++i) {
        if (skin->joint_node_indices[i] == target_node_index) {
            return (int)i;
        }
    }
    return -1;
}

void UpdateLayerTime(C_AnimatedMesh& animatedMesh, AnimationLayer& layer, float dt, float playbackSpeed)
{
	// Update the current animation time
    layer.currentAnimationTime += dt * playbackSpeed;
	float duration = GetAnimationDuration(animatedMesh, layer.currentAnimation);
    if (layer.currentAnimationTime > duration)
    {
        if (layer.isCurrentLooping)
            layer.currentAnimationTime = fmod(layer.currentAnimationTime, duration);
        else
			layer.currentAnimationTime = duration; // Clamp to end of animation if not looping
    }

	// Update previous animation time and blending time if blending
    if (layer.isBlending && layer.previousAnimation >= 0)
    {
		layer.blendTime += dt * playbackSpeed;
		layer.previousAnimationTime += dt * playbackSpeed;
		duration = GetAnimationDuration(animatedMesh, layer.previousAnimation);
        if (layer.previousAnimationTime > duration)
        {
            if (layer.isPreviousLooping)
                layer.previousAnimationTime = fmod(layer.previousAnimationTime, duration);
            else
                layer.previousAnimationTime = duration; // Clamp to end of animation if not looping
        }

        float blendFactor = glm::clamp(layer.blendTime / layer.blendDuration, 0.f, 1.f);
        if (blendFactor >= 1.0f)
        {
            layer.blendTime = 0.0f;
            layer.previousAnimation = -1;
            layer.isBlending = false;
        }
    }
}

void CalculateLayerPose(Asset* asset, AnimationLayer& layer, std::vector<BoneTransform>& currentPose)
{
    // Sample and interpolate animation for the current pose
    AnimationInterpolation(asset, asset->animations[layer.currentAnimation], layer.currentAnimationTime, currentPose);

	// If blending, also sample the previous animation and blend the two poses together
    if (layer.isBlending && layer.previousAnimation >= 0)
    {
        std::vector<BoneTransform> previousPose = currentPose; // Just for default values
        AnimationInterpolation(asset, asset->animations[layer.previousAnimation], layer.previousAnimationTime, previousPose);
		float blendFactor = glm::clamp(layer.blendTime / layer.blendDuration, 0.f, 1.f);
        BlendPoses(previousPose, currentPose, blendFactor, currentPose);
	}
}

void CalculateWorldMatrices(Asset* asset, int boneIndex, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices)
{
    // Calculate world matrix from local matrix and parent world matrix
	worldJointMatrices[boneIndex] = parentMatrix * localJointMatrices[boneIndex];

    // Recursively call for all children
    for (size_t i = 0; i < asset->skins[0].bones[boneIndex].child_count; ++i)
		CalculateWorldMatrices(asset, asset->skins[0].bones[boneIndex].children_indices[i], worldJointMatrices[boneIndex], localJointMatrices, worldJointMatrices);
}

void AnimationInterpolation(Asset* asset, Animation& animation, float animationTime, std::vector<BoneTransform>& pose) 
{
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
            if (animationTime < sampler.inputs[j + 1])
            {
                firstKeyframe = j;
                secondKeyframe = j + 1;
                break;
            }
        }
        // If no keyframes are found, we're past the end of the animation, use last two keyframes
        if (firstKeyframe == 0 && secondKeyframe == 0)
        {
            firstKeyframe = sampler.input_count - 2;
            secondKeyframe = sampler.input_count - 1;
		}
        interpolationFactor = glm::clamp((animationTime - sampler.inputs[firstKeyframe]) / (sampler.inputs[secondKeyframe] - sampler.inputs[firstKeyframe]), 0.f, 1.f);

        // Interpolate the transformations to get local matrices for each joint, 0 = translation, 1 = rotation, 2 = scale
        int jointIndex = find_bone_index(&asset->skins[0], channel.target_node_index);
        if (jointIndex == -1) continue; // Channel is useless, not affecting a joint
        if (channel.target_path == 0)
        {
            glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
            glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
            pose[jointIndex].translation = glm::mix(start, end, interpolationFactor);
        }
        else if (channel.target_path == 1)
        {
            glm::quat start = glm::quat(sampler.outputs[firstKeyframe * 4 + 3], sampler.outputs[firstKeyframe * 4], sampler.outputs[firstKeyframe * 4 + 1], sampler.outputs[firstKeyframe * 4 + 2]);
            glm::quat end = glm::quat(sampler.outputs[secondKeyframe * 4 + 3], sampler.outputs[secondKeyframe * 4], sampler.outputs[secondKeyframe * 4 + 1], sampler.outputs[secondKeyframe * 4 + 2]);
            pose[jointIndex].rotation = glm::slerp(start, end, interpolationFactor);
        }
        else if (channel.target_path == 2)
        {
            glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
            glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
            pose[jointIndex].scale = glm::mix(start, end, interpolationFactor);
        }
    }
}

void BlendPoses(const std::vector<BoneTransform>& poseA, const std::vector<BoneTransform>& poseB, float blendFactor, std::vector<BoneTransform>& blendedPose)
{
    // Just blend em together
    for (size_t i = 0; i < poseA.size(); ++i)
    {
        blendedPose[i].translation = glm::mix(poseA[i].translation, poseB[i].translation, blendFactor);
        blendedPose[i].rotation = glm::slerp(poseA[i].rotation, poseB[i].rotation, blendFactor);
        blendedPose[i].scale = glm::mix(poseA[i].scale, poseB[i].scale, blendFactor);
    }
}



// layered animation
void SetBoneMask(C_AnimatedMesh& animatedMesh, int boneIndex)
{
    // This bone is fully in the layer
	animatedMesh.boneMask[boneIndex] = 1.0f;

    // Recursively set all the children bones in the mask
    for (size_t i = 0; i < animatedMesh.asset->skins[0].bones[boneIndex].child_count; ++i)
		SetBoneMask(animatedMesh, animatedMesh.asset->skins[0].bones[boneIndex].children_indices[i]);
}

void CreateUpperBodyLayer(C_AnimatedMesh& animatedMesh, const char* splitJointName)
{
	// Initialise bone mask with 0s, then find the index of the joint splitting upper/lower body
	animatedMesh.boneMask.assign(animatedMesh.joint_count, 0.0f);

    int splitJointIndex = -1;
    for (size_t i = 0; i < animatedMesh.asset->skins[0].joint_count; ++i)
    {
        if (strcmp(animatedMesh.asset->skins[0].bones[i].name, splitJointName) == 0)
        {
            splitJointIndex = (int)i;
            break;
        }
	}

    // If the split joint is found, all its children will be upper body, so set all mask values to 1
    if (splitJointIndex != -1)
    {
        animatedMesh.boneMask[splitJointIndex] = 0.5f;
		SetBoneMask(animatedMesh, splitJointIndex);
    }
}