#include "stdafx.h"
#include "Renderer.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	Renderer sample(1280, 720, L"D3D12 Hello Window");
	return Win32Application::Run(&sample, hInstance, nCmdShow);
}