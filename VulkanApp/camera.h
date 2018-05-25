#ifndef _CAMERA_H_
#define _CAMERA_H_

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>

class Camera
{
public:
	Camera();

	void MoveForward(float speed);
	void MoveBackward(float speed);
	void MoveLeft(float speed);
	void MoveRight(float speed);

	void TurnUp(float speed) { rotation_.x += speed; }
	void TurnDown(float speed) { rotation_.x -= speed; }
	void TurnLeft(float speed) { rotation_.z += speed; }
	void TurnRight(float speed) { rotation_.z -= speed; }

	inline void SetPosition(glm::vec3 pos) { position_ = pos; }
	inline glm::vec3 GetPosition() { return position_; }

	inline void SetRotation(glm::vec3 rot) { rotation_ = rot; }
	inline glm::vec3 GetRotation() { return rotation_; }

	inline void SetSpeed(float speed) { speed_ = speed; }
	inline float GetSpeed() { return speed_; }

	glm::mat4 GetViewMatrix();
	glm::mat4 GetViewMatrixVerticalOnly();
	glm::mat4 GetProjectionMatrix();

	inline void SetViewDimensions(float width, float height) { view_width_ = width; view_height_ = height; }
	inline void GetViewDimensions(float& width, float& height) { width = view_width_; height = view_height_; }

	inline void SetFieldOfView(float fov) { fov_ = fov; }
	inline float GetFieldOfView() { return fov_; }
	
protected:
	glm::vec3 position_;
	glm::vec3 rotation_;

	float speed_;

	float view_width_, view_height_;
	float fov_;

};

#endif