#include "Mode.hpp"

#include "Scene.hpp"
#include "WalkMesh.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//player info:
	struct Player {
		WalkPoint at;
		//transform is at player's feet and will be yawed by a& d:
		Scene::Transform *transform = nullptr;
		//camera is offset behind player's head and oved by mouse up/down motion:
		Scene::Camera *camera = nullptr;

		float speed = 0.0f;
	} player;

	Scene::Transform* lUpperArm = nullptr;
	Scene::Transform* rUpperArm = nullptr;
	Scene::Transform* lLowerArm = nullptr;
	Scene::Transform* rLowerArm = nullptr;
	Scene::Transform* lThigh = nullptr;
	Scene::Transform* rThigh = nullptr;
	Scene::Transform* lCalf = nullptr;
	Scene::Transform* rCalf = nullptr;
	glm::quat lUpperArmRotation;
	glm::quat rUpperArmRotation;
	glm::quat lLowerArmRotation;
	glm::quat rLowerArmRotation;
	glm::quat lThighRotation;
	glm::quat rThighRotation;
	glm::quat lCalfRotation;
	glm::quat rCalfRotation;


	double total_time_elapsed = 0.0f;
	bool won_game = false;

};
