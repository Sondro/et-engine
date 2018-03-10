/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/rendering/rendercontext.h>

#if (ET_PLATFORM_WIN)

#include <et/rendering/vulkan/vulkan_renderer.h>
#include <et/rendering/base/helpers.h>

#include <et/app/application.h>
#include <windowsx.h>

namespace et {

class RenderContextPrivate
{
public:
	Input::PointerInputSource pointerInput;
	Input::KeyboardInputSource keyboardInput;
	LRESULT mainWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam);
	PointerInputInfo mouseInputInfo(uint32_t, uint32_t, uint32_t mask);
	void captureMouse();
	void releaseMouse();

	HWND mainWindow = nullptr;
	RECT clientRect{ };
	RendererFrame currentFrame;
	uint32_t mouseCaptureCounter = 0;
	bool resizeScheduled = false;
};

RenderContext::RenderContext() {
	ET_PIMPL_INIT(RenderContext);
}

RenderContext::~RenderContext() {
	ET_PIMPL_FINALIZE(RenderContext);
}

void RenderContext::init(const ApplicationParameters& appParams, const RenderContextParameters& rcParams) {
	_private->mainWindow = reinterpret_cast<HWND>(application().context().objects[0]);
	SetWindowLongPtr(_private->mainWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(_private));

	if (appParams.renderingAPI == RenderingAPI::Vulkan)
	{
		_renderer = VulkanRenderer::Pointer::create();
	}
	else
	{
		ET_FAIL("Invalid or unsupported rendering api provided");
	}
	_renderer->init(rcParams);
	renderhelper::init(_renderer);

	GetClientRect(_private->mainWindow, &_private->clientRect);

	ShowWindow(_private->mainWindow, SW_SHOW);
	SetForegroundWindow(_private->mainWindow);
	SetFocus(_private->mainWindow);
}

void RenderContext::shutdown() {
	renderhelper::release();
	_renderer->destroy();
	_renderer.release();
}

bool RenderContext::beginRender() {
	if (_private->resizeScheduled)
	{
		GetClientRect(_private->mainWindow, &_private->clientRect);
		_renderer->resize(vec2i(_private->clientRect.right - _private->clientRect.left,
			_private->clientRect.bottom - _private->clientRect.top));
		_private->resizeScheduled = false;
	}

	_private->currentFrame = _renderer->allocateFrame();
	return (_private->currentFrame.identifier != 0);
}

void RenderContext::endRender() {
	_renderer->submitFrame(_private->currentFrame);

	if (_renderer->parameters().multithreadingEnabled == false)
		_renderer->present();
}

void RenderContext::performResizing(const vec2i&) {
	_private->resizeScheduled = true;
}

LRESULT RenderContextPrivate::mainWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{
	case WM_CLOSE:
	case WM_QUIT:
	{
		application().quit(0);
		return 0;
	}

	/*
	 * Keyboard messages
	 */
	case WM_KEYDOWN:
	{
		keyboardInput.keyPressed(static_cast<uint32_t>(wParam));
		return 0;
	}
	case WM_KEYUP:
	{
		keyboardInput.keyReleased(static_cast<uint32_t>(wParam));
		return 0;
	}
	case WM_UNICHAR:
	{
		if (wParam >= ET_SPACE)
		{
			wchar_t wChars[2] = { static_cast<wchar_t>(wParam), 0 };
			keyboardInput.charactersEntered(unicodeToUtf8(wChars));

		}
		return 0;
	}
	case WM_CHAR:
	{
		if (wParam >= ET_SPACE)
		{
			wchar_t wChars[2] = { static_cast<wchar_t>(wParam), 0 };
			keyboardInput.charactersEntered(unicodeToUtf8(wChars));
		}
		return 0;

	}
	/*
	 * Mouse messages
	 */
	case WM_LBUTTONDOWN:
	{
		pointerInput.pointerPressed(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::General));
		return 0;
	}
	case WM_LBUTTONUP:
	{
		pointerInput.pointerReleased(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::General));
		return 0;
	}
	case WM_RBUTTONDOWN:
	{
		pointerInput.pointerPressed(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::RightButton));
		return 0;
	}
	case WM_RBUTTONUP:
	{
		pointerInput.pointerReleased(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::RightButton));
		return 0;
	}
	case WM_MBUTTONDOWN:
	{
		pointerInput.pointerPressed(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::MiddleButton));
		return 0;
	}
	case WM_MBUTTONUP:
	{
		pointerInput.pointerReleased(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), PointerTypeMask::MiddleButton));
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		uint32_t mask = 0;
		mask |= (wParam & MK_LBUTTON) ? PointerTypeMask::General : 0;
		mask |= (wParam & MK_RBUTTON) ? PointerTypeMask::RightButton : 0;
		mask |= (wParam & MK_MBUTTON) ? PointerTypeMask::MiddleButton : 0;
		pointerInput.pointerMoved(mouseInputInfo(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), mask));
		return 0;
	}
	case WM_MOUSEWHEEL:
	{
		uint32_t mask = 0;
		mask |= (wParam & MK_LBUTTON) ? PointerTypeMask::General : 0;
		mask |= (wParam & MK_RBUTTON) ? PointerTypeMask::RightButton : 0;
		mask |= (wParam & MK_MBUTTON) ? PointerTypeMask::MiddleButton : 0;

		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		ScreenToClient(mainWindow, &pt);

		PointerInputInfo inputInfo = mouseInputInfo(pt.x, pt.y, mask);
		inputInfo.scroll = vec2(0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA));
		pointerInput.pointerScrolled(inputInfo);
		return 0;
	}

	/*
	 * Application messages
	 */
	case WM_ACTIVATE:
	{
		application().setActive(wParam != WA_INACTIVE);
		return 0;
	}
	case WM_SIZE:
	{
		application().resizeContext(vec2i(LOWORD(lParam), HIWORD(lParam)));
		return 0;
	}

	default:
		break;
	}
	return DefWindowProc(wnd, msg, wParam, lParam);
}

PointerInputInfo RenderContextPrivate::mouseInputInfo(uint32_t x, uint32_t y, uint32_t mask) {
	PointerInputInfo info;
	info.id = 1;
	info.type = mask;
	info.origin = PointerOrigin::Mouse;
	info.pos.x = static_cast<float>(x);
	info.pos.y = static_cast<float>(y);
	info.normalizedPos.x = 2.0f * info.pos.x / static_cast<float>(clientRect.right - clientRect.left) - 1.0f;
	info.normalizedPos.y = 1.0f - 2.0f * info.pos.y / static_cast<float>(clientRect.bottom - clientRect.top);
	info.timestamp = queryContinuousTimeInSeconds();
	return info;
}

void RenderContextPrivate::captureMouse() {
	++mouseCaptureCounter;
	SetCapture(mainWindow);
}

void RenderContextPrivate::releaseMouse() {
	ET_ASSERT(mouseCaptureCounter > 0);
	--mouseCaptureCounter;
	if (mouseCaptureCounter == 0)
	{
		ReleaseCapture();
	}
}


/*
* Windows procedure
*/
LRESULT WINAPI mainWindowProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	LONG_PTR context = GetWindowLongPtr(wnd, GWLP_USERDATA);
	return (context != 0) ?
		reinterpret_cast<RenderContextPrivate*>(context)->mainWindowProc(wnd, msg, wParam, lParam) :
		DefWindowProc(wnd, msg, wParam, lParam);
}

}

#endif // ET_PLATFORM_WIN
