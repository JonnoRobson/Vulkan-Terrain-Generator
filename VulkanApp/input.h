#ifndef _INPUT_H_
#define _INPUT_H_

#include <GLFW\glfw3.h>

class Input
{
public:
	Input()
	{
		for (int i = 0; i < GLFW_KEY_LAST; i++)
		{
			key_states_[i] = false;
		}
	}

	void SetMouseButtonUp(int button) { mouse_button_states_[button] = false; }
	void SetMouseButtonDown(int button) { mouse_button_states_[button] = true; }
	bool IsMouseButtonPressed(int button) { return mouse_button_states_[button]; }

	void SetKeyDown(int key) { key_states_[key] = true; }
	void SetKeyUp(int key) { key_states_[key] = false; }
	bool IsKeyPressed(int key) { return key_states_[key]; }

	void SetCursorPosition(double x_pos, double y_pos) { cursor_x_pos_ = x_pos; cursor_y_pos_ = y_pos; }
	void GetCursorPosition(double& x_pos, double& y_pos) { x_pos = cursor_x_pos_; y_pos = cursor_y_pos_; }

protected:
	double cursor_x_pos_;
	double cursor_y_pos_;

	bool key_states_[GLFW_KEY_LAST];
	bool mouse_button_states_[4];

};

#endif