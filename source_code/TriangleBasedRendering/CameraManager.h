//--------------------------------------------------------------------------------------
// File: CameraManager.h
//
// Store view and projection matrices used by shaders
// The code is modified form www.gamedev.net
// To do:
// Some functions don't work correctly, it should be fixed.
//--------------------------------------------------------------------------------------
#pragma once
#include <DirectXMath.h>
#include "DirectXMathConverter.h"
#include <Windows.h>

class GCamera
{
public:
	// Constructs default camera looking at 0,0,0
	// placed at 0,0,-1 with up vector 0,1,0 (note that mUp is NOT a vector - it's vector's end)
	GCamera(void);
	// Create camera, based on another one
	GCamera(const GCamera& camera);
	// Copy all camera's parameters
	GCamera& operator=(const GCamera& camera);
	~GCamera(void) {}

private:
	// Initialize camera's View matrix from mPosition, mTarget and mUp coordinates
	void lookatViewMatrix();
	void buildViewMatrix();
public:
	// Initialize camera's perspective Projection matrix
	void InitProjMatrix(const float angle, const float client_width, const float client_height,
		const float nearest, const float farthest);
	// Initialize camera's orthogonal projection
	void InitOrthoMatrix(const float client_width, const float client_height,
		const float near_plane, const float far_plane);

	// Resize matrices when window size changes
	void OnResize(float new_width, float new_height);

	///////////////////////////////////////////////
	/*** View matrix transformation interfaces ***/
	///////////////////////////////////////////////

	// Move camera
	void Move(DirectX::XMFLOAT3 direction);
	// Rotate camera around `axis` by `degrees`. Camera's position is a 
	// pivot point of rotation, so it doesn't change
	void Rotate(DirectX::XMFLOAT3 axis, float degrees);
	// Set camera position coordinates
	void Position(DirectX::XMFLOAT3& new_position);
	// Get camera position coordinates
	const DirectX::XMFLOAT3& Position() const { return mPosition; }
	// Change camera target position
	void Target(DirectX::XMFLOAT3 new_target);
	// Get camera's target position coordinates
	const DirectX::XMFLOAT3& Target() const { return mTarget; }
	// Get camera's up vector
	const DirectX::XMFLOAT3 Up();
	// Get camera's look at target vector
	DirectX::XMFLOAT3 LookAtTarget();
	// Returns transposed camera's View matrix	
	const DirectX::XMFLOAT4X4 View() { return GMathMF(XMMatrixTranspose(GMathFM(mView))); }

	/////////////////////////////////////////////////////
	/*** Projection matrix transformation interfaces ***/
	/////////////////////////////////////////////////////

	// Set view frustum's angle
	void Angle(float angle);
	// Get view frustum's angle
	const float& Angle() const { return mAngle; }

	// Set nearest culling plane distance from view frustum's projection plane
	void NearestPlane(float nearest);
	// Set farthest culling plane distance from view frustum's projection plane
	void FarthestPlane(float farthest);


	float GetNearestPlane() { return mNearest; }
	float GetFarthestPlane() { return mFarthest; }

	// Returns transposed camera's Projection matrix
	const DirectX::XMFLOAT4X4 Proj() { return GMathMF(XMMatrixTranspose(GMathFM(mProj))); }
	// Returns transposed orthogonal camera matrix
	const DirectX::XMFLOAT4X4 Ortho() { return GMathMF(XMMatrixTranspose(GMathFM(mOrtho))); }

	const DirectX::XMFLOAT4X4 ProjView() { return GMathMF(XMMatrixTranspose(GMathFM(mView)*(GMathFM(mProj)))); }


	const DirectX::XMFLOAT4X4 InvScreenProjView() {


		DirectX::XMFLOAT4X4 matScreen = DirectX::XMFLOAT4X4(2 / mClientWidth, 0, 0, 0, 0, -2 / mClientHeight, 0, 0, 0, 0, 1, 0, -1, 1, 0, 1);
		return GMathMF(XMMatrixTranspose(GMathFM(matScreen)*XMMatrixInverse(&XMMatrixDeterminant(GMathFM(mProj)), GMathFM(mProj))*
			XMMatrixInverse(&XMMatrixDeterminant(GMathFM(mView)), GMathFM(mView))));
	}
	const DirectX::XMFLOAT4X4 InvProj() {

		return GMathMF(XMMatrixTranspose(XMMatrixInverse(&XMMatrixDeterminant(GMathFM(mProj)), GMathFM(mProj))));
	}



	void KeyDown(UINT key);
	void KeyUp(UINT key);

	void Update();

	void InputPress(float x, float y);

	void InputMove(float x, float y);

	void InputRelease();

	void SetSpeed(float speed) { mSpeed = speed; }

private:
	const float ROTATION_GAIN = 0.004f;
	const float MOVEMENT_GAIN = 0.1f;
	float mSpeed;
	float m_pitch, m_yaw;			// orientation Euler angles in radians
	/*** Input parameters ***/
	// properties of the Look control
	bool m_lookInUse;			    // specifies whether the look control is in use
	int m_lookPointerID;		    // id of the pointer in this control
	DirectX::XMFLOAT2 m_lookLastPoint;		    // last point (from last frame)
	DirectX::XMFLOAT2 m_lookLastDelta;		    // for smoothing
	bool m_forward;
	bool m_back;
	bool m_left;
	bool m_right;



	/*** Camera parameters ***/
	DirectX::XMFLOAT3 mPosition;		// Camera's coordinates
	DirectX::XMFLOAT3 mTarget;		// View target's coordinates
	DirectX::XMFLOAT3 mUp;			// Camera's up vector end coordinates
	DirectX::XMFLOAT3 mLook;
	DirectX::XMFLOAT3 mForward;
	DirectX::XMFLOAT3 mRight;
	/*** Projection parameters ***/
	float mAngle;			// Angle of view frustum
	float mClientWidth;		// Window's width
	float mClientHeight;	// Window's height
	float mNearest;			// Nearest view frustum plane
	float mFarthest;		// Farthest view frustum plane




	DirectX::XMFLOAT4X4  mView;		// View matrix
	DirectX::XMFLOAT4X4	mProj;		// Projection matrix
	DirectX::XMFLOAT4X4	mOrtho;		// Ortho matrix for drawing without transformation
};

