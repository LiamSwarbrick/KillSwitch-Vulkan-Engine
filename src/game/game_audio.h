#pragma once

class Scene;
struct CameraInfo;
struct GameAudio;

GameAudio* GameAudio_Init();
void GameAudio_Destroy(GameAudio* audio);

void GameAudio_Update(
	GameAudio* audio,
	Scene& scene,
	const CameraInfo& listener_camera,
	float dt);
