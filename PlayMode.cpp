#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <random>

GLuint track_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > track_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("track.pnct"));
	track_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > track_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("track.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = track_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = track_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

WalkMesh const *walkmesh = nullptr;
Load< WalkMeshes > track_walkmeshes(LoadTagDefault, []() -> WalkMeshes const * {
	WalkMeshes *ret = new WalkMeshes(data_path("track.w"));
	walkmesh = &ret->lookup("WalkMesh");
	return ret;
});

PlayMode::PlayMode() : scene(*track_scene) {
	//create a player transform:
	//scene.transforms.emplace_back();
	//player.transform = &scene.transforms.back(); <-- player transform set in track_scene


	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Waist") player.transform = &transform;
		else if(transform.name == "L_UpperArm") lUpperArm = &transform;
		else if(transform.name == "R_UpperArm") rUpperArm = &transform;
		else if(transform.name == "L_LowerArm") lLowerArm = &transform;
		else if(transform.name == "R_LowerArm") rLowerArm = &transform;
		else if(transform.name == "L_ThighArm") lThigh = &transform;
		else if(transform.name == "L_ThighArm") rThigh = &transform;
		else if(transform.name == "L_CalfArm") lCalf = &transform;
		else if(transform.name == "L_CalfArm") rCalf = &transform;
	}

	if (player.transform == nullptr) throw std::runtime_error("Player not found.");
	if (lUpperArm == nullptr) throw std::runtime_error("Left Upper Arm not found.");
	if (rUpperArm == nullptr) throw std::runtime_error("Right Upper Arm not found.");
	if (lLowerArm == nullptr) throw std::runtime_error("Left Lower Arm not found.");
	if (rLowerArm == nullptr) throw std::runtime_error("Right Lower Arm not found.");
	if (lThighArm == nullptr) throw std::runtime_error("Left Thigh not found.");
	if (rThighArm == nullptr) throw std::runtime_error("Right Thigh not found.");
	if (lCalfArm == nullptr) throw std::runtime_error("Left Calf not found.");
	if (rCalfArm == nullptr) throw std::runtime_error("Right Calf not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();



	//set start position
	player.transform->position = vec3(0.0f, 0.0f, 0.0f);

	//create a player camera attached to a child of the player transform:
	scene.transforms.emplace_back();
	scene.cameras.emplace_back(&scene.transforms.back());
	player.camera = &scene.cameras.back();
	player.camera->fovy = glm::radians(60.0f);
	player.camera->near = 0.01f;

	player.camera->transform->parent = player.transform;

	//offset position to behind player
	player.camera->transform->position = glm::vec3(0.0f, -5.0f, 5.0f);

	//rotate camera facing direction (-z) to player facing direction (+y):
	player.camera->transform->rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	//start player walking at nearest walk point:
	player.at = walkmesh->nearest_walk_point(player.transform->position);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			glm::vec3 upDir = walkmesh->to_world_smooth_normal(player.at);
			player.transform->rotation = glm::angleAxis(-motion.x * player.camera->fovy, upDir) * player.transform->rotation;

			float pitch = glm::pitch(player.camera->transform->rotation);
			pitch += motion.y * player.camera->fovy;
			//camera looks down -z (basically at the player's feet) when pitch is at zero.
			pitch = std::min(pitch, 0.95f * 3.1415926f);
			pitch = std::max(pitch, 0.05f * 3.1415926f);
			player.camera->transform->rotation = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//player walking:
	{
		total_time_elapsed += elapsed;

		if(won_game || player.transform->position.y >= 10.0f) { //reached end
			won_game = true;
			return;
		}

		//combine inputs into a move:
		constexpr float PlayerVel = 1.0f;
		glm::vec2 move = glm::vec2(0.0f);
		//if (left.pressed && !right.pressed) move.x =-1.0f;
		//if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) player.speed -= 1.0f;
		if (!down.pressed && up.pressed) player.speed += 1.0f;

		if(!down.pressed && !up.pressed) {
			player.speed -= 1.0f;
			player.speed = std::clamp(player.speed, 0.0f, 10.0f);
		} else {
			player.speed = std::clamp(player.speed. -10.0f, 10.0f);
		}


		//procedural running animation by speed
		float speed = std::abs(player.speed);
		glm::quat limbBend = lLowerArmRotaion = glm::angleAxis(speed*-(3.141592f/2.0f), glm::vec3(1.0f, 0.0f, 0.0f));;
		lLowerArmRotaion = limbBend;
		rLowerArmRotaion = limbBend;
		lCalfRotaion = limbBend;
		rCalfRotaion = limbBend;
		lUpperArmRotaion = glm::angleAxis(speed*glm::radians(60.0f)*sin(total_time_elapsed*6.28318530718f), glm::vec3(1.0f, 0.0f, 0.0f));
		rUpperArmRotaion = glm::angleAxis(speed*glm::radians(60.0f)*sin(total_time_elapsed*6.28318530718f + 3.141592f), glm::vec3(1.0f, 0.0f, 0.0f));
		lThighArmRotaion = glm::angleAxis(speed*glm::radians(60.0f)*sin(total_time_elapsed*6.28318530718f + 3.141592f), glm::vec3(1.0f, 0.0f, 0.0f));
		rThighArmRotaion = glm::angleAxis(speed*glm::radians(60.0f)*sin(total_time_elapsed*6.28318530718f), glm::vec3(1.0f, 0.0f, 0.0f));
		

		glm::vec3 remain = player.transform->make_local_to_world() * glm::vec4(0.0f, player.speed, 0.0f, 0.0f);

		//get move in world cordinate system:

		//using a for() instead of a while() here so that if walkpoint gets stuck in
		// some awkward case, code will not infinite loop:
		for (uint32_t iter = 0; iter < 10; ++iter) {
			if (remain == glm::vec3(0.0f)) break;
			WalkPoint end;
			float time;
			walkmesh->walk_in_triangle(player.at, remain, &end, &time);
			player.at = end;
			if (time == 1.0f) {
				//finished within triangle:
				remain = glm::vec3(0.0f);
				break;
			}
			//some step remains:
			remain *= (1.0f - time);
			//try to step over edge:
			glm::quat rotation;
			if (walkmesh->cross_edge(player.at, &end, &rotation)) {
				//stepped to a new triangle:
				player.at = end;
				//rotate step to follow surface:
				remain = rotation * remain;
			} else {
				//ran into a wall, bounce / slide along it:
				glm::vec3 const &a = walkmesh->vertices[player.at.indices.x];
				glm::vec3 const &b = walkmesh->vertices[player.at.indices.y];
				glm::vec3 const &c = walkmesh->vertices[player.at.indices.z];
				glm::vec3 along = glm::normalize(b-a);
				glm::vec3 normal = glm::normalize(glm::cross(b-a, c-a));
				glm::vec3 in = glm::cross(normal, along);

				//check how much 'remain' is pointing out of the triangle:
				float d = glm::dot(remain, in);
				if (d < 0.0f) {
					//bounce off of the wall:
					remain += (-1.25f * d) * in;
				} else {
					//if it's just pointing along the edge, bend slightly away from wall:
					remain += 0.01f * d * in;
				}
			}
		}

		if (remain != glm::vec3(0.0f)) {
			std::cout << "NOTE: code used full iteration budget for walking." << std::endl;
		}

		//update player's position to respect walking:
		player.transform->position = walkmesh->to_world_point(player.at);

		{ //update player's rotation to respect local (smooth) up-vector:
			
			glm::quat adjust = glm::rotation(
				player.transform->rotation * glm::vec3(0.0f, 0.0f, 1.0f), //current up vector
				walkmesh->to_world_smooth_normal(player.at) //smoothed up vector at walk location
			);
			player.transform->rotation = glm::normalize(adjust * player.transform->rotation);
		}


		/*
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 forward = -frame[2];

		camera->transform->position += move.x * right + move.y * forward;
		*/
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//waist bobbing animation. purely visual so hopefully doesn't affect gameplay behavior
	float speed = std::abs(player.speed);
	float offset = ()*std::sin(6.28318530718f*time_elapsed);
	player.transform.z -= ((1.92234f - 1.83241f)*speed)*std::sin(6.28318530718f*time_elapsed);



	//update camera aspect ratio for drawable:
	player.camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*player.camera);

	/* In case you are wondering if your walkmesh is lining up with your scene, try:
	{
		glDisable(GL_DEPTH_TEST);
		DrawLines lines(player.camera->make_projection() * glm::mat4(player.camera->transform->make_world_to_local()));
		for (auto const &tri : walkmesh->triangles) {
			lines.draw(walkmesh->vertices[tri.x], walkmesh->vertices[tri.y], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.y], walkmesh->vertices[tri.z], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
			lines.draw(walkmesh->vertices[tri.z], walkmesh->vertices[tri.x], glm::u8vec4(0x88, 0x00, 0xff, 0xff));
		}
	}
	*/

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));



		constexpr float H = 0.09f;
		if(won_game) {
			lines.draw_text("You won with a time of " + std::to_string(time_elapsed),
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));

		} else {
			lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
				glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text("Mouse motion looks; WASD moves; escape ungrabs mouse",
				glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		}
	}
	player.transform.z += offset;
	GL_ERRORS();
}
