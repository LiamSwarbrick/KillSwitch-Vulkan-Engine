#include "animation.h"
#include "components.h"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"

void Animation_Update(AdvEng::ECS* ecs, float dt)
{
    auto view = ecs->GetView<C_Animator>();

    for (AdvEng::EntityID e : view)
    {
        auto& animator = ecs->GetComponent<C_Animator>(e);
        Asset* asset = animator.asset;

        // if (!animator.isPlaying || !asset || animator.currentAnimation < 0)
        //      continue;

        Animation& animation = asset->animations[animator.currentAnimation];

		// Update the animation time
        animator.animationTime += dt;
        if (animator.isLooping)
            animator.animationTime = fmod(animator.animationTime, GetDuration(animator));

		// Vectors to store the interpolated transformations for each channel
        // PUT THESE IN THE ANIMATOR COMPONENT WHEN BLENDING
		int animatedBoneCount = animation.channel_count / 3; // Each node has translation, rotation, scale
		std::vector<glm::vec3> translations(animatedBoneCount);
		std::vector<glm::quat> rotations(animatedBoneCount);
        std::vector<glm::vec3> scales(animatedBoneCount);
		std::vector<int> nodeIndices(animatedBoneCount);
        // Might want to fill with default values so if something breaks it shouldn't crash

        for (size_t i = 0; i < animation.channel_count; ++i)
        {
            AnimationChannel& channel = animation.channels[i];
            AnimationSampler& sampler = animation.samplers[channel.sampler_index];
            int nodeIndex = channel.target_node_index;
            nodeIndices[i / 3] = nodeIndex;


            // Find the keyframes and interpolation value from the current animation time
            int firstKeyframe = 0;
            int secondKeyframe = 0;
            float interpolationFactor = 0.0f;
            for (size_t j = 0; j < sampler.input_count; j++)
            {
                if (animator.animationTime < sampler.inputs[j + 1])
                {
                    firstKeyframe = j;
                    secondKeyframe = j + 1;
                    break;
                }
            }
            interpolationFactor = (animator.animationTime - sampler.inputs[firstKeyframe]) / (sampler.inputs[secondKeyframe] - sampler.inputs[firstKeyframe]);

			// Interpolate the transformations to get local matrices for each joint
            if (channel.target_path == 0)
            {
                glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
                glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
                translations[i / 3] = glm::mix(start, end, interpolationFactor);
            }
            else if (channel.target_path == 1)
            {
                glm::quat start = glm::quat(sampler.outputs[firstKeyframe * 4 + 3], sampler.outputs[firstKeyframe * 4], sampler.outputs[firstKeyframe * 4 + 1], sampler.outputs[firstKeyframe * 4 + 2]);
                glm::quat end = glm::quat(sampler.outputs[secondKeyframe * 4 + 3], sampler.outputs[secondKeyframe * 4], sampler.outputs[secondKeyframe * 4 + 1], sampler.outputs[secondKeyframe * 4 + 2]);
                rotations[i / 3] = glm::slerp(start, end, interpolationFactor);
            }
            else if (channel.target_path == 2)
            {
                glm::vec3 start = glm::vec3(sampler.outputs[firstKeyframe * 3], sampler.outputs[firstKeyframe * 3 + 1], sampler.outputs[firstKeyframe * 3 + 2]);
                glm::vec3 end = glm::vec3(sampler.outputs[secondKeyframe * 3], sampler.outputs[secondKeyframe * 3 + 1], sampler.outputs[secondKeyframe * 3 + 2]);
                scales[i / 3] = glm::mix(start, end, interpolationFactor);
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
		bool isRoot = true;

		// Find root joints and calculate world matrices recursively from them / CURRENTLY USES BLENDER HIERARCHY
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            for (int j = 0; j < animatedBoneCount; ++j)
            {
                if (asset->nodes[nodeIndices[i]].parent_index == nodeIndices[j])
                    isRoot = false;
            }

            if (isRoot)
				CalculateWorldMatrices(asset, i, nodeIndices, glm::mat4(1.0f), localJointMatrices, worldJointMatrices);      
		}

		// Resize the final joint matrices vector and apply inverse bind pose
        animator.final_joint_matrices.resize(animatedBoneCount);
        for (int i = 0; i < animatedBoneCount; ++i)
        {
            int jointIndex = nodeIndices[i];
            glm::mat4 inverseBindMatrix = glm::make_mat4(asset->skins[0].inverse_bind_matrices + jointIndex * 16); // HAVE TO ASSUME ONE SKIN FOR NOW
            animator.final_joint_matrices[i] = worldJointMatrices[i] * inverseBindMatrix;
        }

    }
    return;
}

void Start(C_Animator& animator, const char* name) {return;} // by name
void Start(C_Animator& animator, int id) { return; } // by index
void Stop(C_Animator& animator) { return; }

// settings
void SetLooping(C_Animator& animator, bool looping) { return; } // ToggleLooping? if so just switch bool

// state checks
bool IsRunning(const C_Animator& animator) { return true; }
bool WillExpire(const C_Animator& animator) { return true; }

// getters
float GetDuration(const C_Animator& animator) { return 0.0f; } // Get first and last keyframes of the current animation and return the difference
float GetCurrentTime(const C_Animator& animator) { return 0.0f; } // Just animationTime

// calculations
void CalculateWorldMatrices(Asset* asset, int boneIndex, const std::vector<int>& nodeIndices, glm::mat4 parentMatrix, const std::vector<glm::mat4>& localJointMatrices, std::vector<glm::mat4>& worldJointMatrices)
{
	// CURRENTLY USES VALUES FROM THE NODES, NOT THE SKIN/BONES, SO IS FRAGILE AND DEPENDS ON THE BLENDER HIERARCHY
    // Calculate world matrix from local matrix and parent world matrix
	Node& node = asset->nodes[nodeIndices[boneIndex]];
	worldJointMatrices[boneIndex] = parentMatrix * localJointMatrices[boneIndex];

    // Recursively call for all children
    for (size_t i = 0; i < node.child_count; ++i)
    {
		int childIndex = node.children_indices[i];
		for (size_t j = 0; j < nodeIndices.size(); ++j)
		{
			if (nodeIndices[j] == childIndex)
				CalculateWorldMatrices(asset, j, nodeIndices, worldJointMatrices[boneIndex], localJointMatrices, worldJointMatrices);
		}
    }
}
