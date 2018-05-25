#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera()
{
	position_ = glm::vec3(0.0f);
	rotation_ = glm::vec3(0.0f);
	speed_ = 100.0f;
	view_width_ = 0.0f;
	view_height_ = 0.0f;
	fov_ = 0.0f;
}

void Camera::MoveForward(float speed)
{
	speed *= speed_;
	glm::vec3 forward = glm::rotateX(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotation_.x));
	forward = glm::rotateY(forward, glm::radians(rotation_.y));
	forward = glm::rotateZ(forward, glm::radians(rotation_.z));

	position_ += forward * speed;
}

void Camera::MoveBackward(float speed)
{
	speed *= speed_;
	glm::vec3 forward = glm::rotateX(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotation_.x));
	forward = glm::rotateY(forward, glm::radians(rotation_.y));
	forward = glm::rotateZ(forward, glm::radians(rotation_.z));

	position_ -= forward * speed;
}

void Camera::MoveLeft(float speed)
{
	speed *= speed_;
	glm::vec3 right = glm::rotateX(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotation_.x));
	right = glm::rotateY(right, glm::radians(rotation_.y));
	right = glm::rotateZ(right, glm::radians(rotation_.z));

	position_ -= right * speed;
}

void Camera::MoveRight(float speed)
{
	speed *= speed_;
	glm::vec3 right = glm::rotateX(glm::vec3(1.0f, 0.0f, 0.0f), glm::radians(rotation_.x));
	right = glm::rotateY(right, glm::radians(rotation_.y));
	right = glm::rotateZ(right, glm::radians(rotation_.z));

	position_ += right * speed;
}

glm::mat4 Camera::GetViewMatrix()
{
	glm::mat4 view_matrix;

	glm::vec3 look_at = glm::rotateX(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotation_.x));
	look_at = glm::rotateY(look_at, glm::radians(rotation_.y));
	look_at = glm::rotateZ(look_at, glm::radians(rotation_.z));
	look_at = look_at + position_;

	view_matrix = glm::lookAt(position_, look_at, glm::vec3(0.0f, 0.0f, 1.0f));

	return view_matrix;
}

glm::mat4 Camera::GetViewMatrixVerticalOnly()
{
	glm::mat4 view_matrix;

	glm::vec3 look_at = glm::rotateX(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(rotation_.x));
	look_at = glm::rotateY(look_at, glm::radians(rotation_.y));
	look_at = glm::rotateZ(look_at, glm::radians(rotation_.z));
	look_at = look_at + glm::vec3(0.0f, 0.0f, position_.z);

	view_matrix = glm::lookAt(glm::vec3(0.0f, 0.0f, position_.z), look_at, glm::vec3(0.0f, 0.0f, 1.0f));

	return view_matrix;
}


glm::mat4 Camera::GetProjectionMatrix()
{
	glm::mat4 proj_matrix;

	glm::mat4 clip =
		glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, -1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.0f, 0.0f, 0.5f, 1.0f);

	proj_matrix = clip * glm::infinitePerspective(fov_, view_width_ / view_height_, 0.1f);
	
	return proj_matrix;
}