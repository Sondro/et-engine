/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/rendering/rendercontext.h>
#include <et/app/application.h>

namespace et {

uint32_t randomInteger(uint32_t limit);

struct NullApplicationDelegate : IApplicationDelegate
{
	ApplicationIdentifier applicationIdentifier() const override {
		return ApplicationIdentifier("com.et.app", "et-app", "et");
	}
};

Application::Application() {
	sharedObjectFactory();
	platformInit();

	log::addOutput(log::ConsoleOutput::Pointer::create());
	_lastQueuedTimeMSec = queryContiniousTimeInMilliSeconds();

	delegate()->setApplicationParameters(_parameters);
	_env.updateDocumentsFolder(_identifier);
	_standardPathResolver.init(_env);

	threading::setMainThreadIdentifier(threading::currentThread());
	platformActivate();

	_backgroundThread.run();
	_renderThread.run();
}

IApplicationDelegate* Application::delegate() {
	if (_delegate == nullptr)
	{
		_delegate = initApplicationDelegate();

		if (_delegate == nullptr)
			_delegate = etCreateObject<NullApplicationDelegate>();

		_identifier = _delegate->applicationIdentifier();
	}

	return _delegate;
}

int Application::run(int argc, char* argv[]) {
#if (ET_DEBUG)
	log::info("[et-engine] Version: %d.%d, running in debug mode.", ET_MAJOR_VERSION, ET_MINOR_VERSION);
#endif

	for (int i = 0; i < argc; ++i)
		_launchParameters.emplace_back(argv[i]);

	return platformRun();
}

void Application::enterRunLoop() {

}

void Application::exitRunLoop() {
}

bool Application::shouldPerformRendering() {
	uint64_t currentTime = queryContiniousTimeInMilliSeconds();
	uint64_t elapsedTime = currentTime - _lastQueuedTimeMSec;

	if ((_fpsLimitMSec > 0) && (elapsedTime < _fpsLimitMSec))
	{
		uint64_t sleepInterval = (_fpsLimitMSec - elapsedTime) +
			(randomInteger(1000) > _fpsLimitMSecFractPart ? 0 : static_cast<uint64_t>(-1));

		threading::sleepMSec(sleepInterval);

		return false;
	}
	_lastQueuedTimeMSec = queryContiniousTimeInMilliSeconds();

	return !_suspended;
}

void Application::performUpdateAndRender() {
	ET_ASSERT(_running && !_suspended);

	uint64_t startTime = queryCurrentTimeInMicroSeconds();
	if (_renderContext.beginRender())
	{
		if (_scheduleResize)
		{
			_renderContext.performResizing(_scheduledSize);
			_delegate->applicationWillResizeContext(_scheduledSize);
			_scheduleResize = false;
		}

		_runLoop.update(_lastQueuedTimeMSec);
		_delegate->update();
		_renderContext.endRender();
	}
	_profiler.frameTime = queryCurrentTimeInMicroSeconds() - startTime;
}

void Application::setFrameRateLimit(size_t value) {
	_fpsLimitMSec = (value == 0) ? 0 : 1000 / value;
	_fpsLimitMSecFractPart = (value == 0) ? 0 : (1000000 / value - 1000 * _fpsLimitMSec);
}

void Application::setActive(bool active) {
	if (!_running || (_active == active)) return;

	_active = active;

	if (active)
	{
		if (_suspended)
			resume();

		_delegate->applicationWillActivate();

		platformActivate();
	}
	else
	{
		_delegate->applicationWillDeactivate();
		platformDeactivate();

		if (_parameters.shouldSuspendOnDeactivate)
			suspend();
	}
}

void Application::resizeContext(const vec2i& size) {
	_scheduledSize = size;
	_scheduleResize = true;
}

void Application::suspend() {
	ET_ASSERT(!_suspended && "Should not be suspended.");

	delegate()->applicationWillSuspend();
	_runLoop.pause();

	platformSuspend();

	_suspended = true;
}

void Application::resume() {
	ET_ASSERT(_suspended && "Should be suspended.");

	delegate()->applicationWillResume();

	_suspended = false;

	platformResume();

	_lastQueuedTimeMSec = queryContiniousTimeInMilliSeconds();
	_runLoop.update(_lastQueuedTimeMSec);
	_runLoop.resume();
}

void Application::stop() {
}

std::string Application::resolveFileName(const std::string& path) {
	std::string result = path;

	if (_customPathResolver.valid())
		result = _customPathResolver->resolveFilePath(path);

	if (!fileExists(result))
		result = _standardPathResolver.resolveFilePath(path);

	return fileExists(result) ? result : path;
}

std::string Application::resolveFolderName(const std::string& path) {
	std::string result;

	if (_customPathResolver.valid())
		result = _customPathResolver->resolveFolderPath(path);

	if (!folderExists(result))
		result = _standardPathResolver.resolveFolderPath(path);

	result = addTrailingSlash(folderExists(result) ? result : path);
	normalizeFilePath(result);

	return result;
}

std::set<std::string> Application::resolveFolderNames(const std::string& path) {
	std::set<std::string> result;

	if (_customPathResolver.valid())
		result = _customPathResolver->resolveFolderPaths(path);

	auto standard = _standardPathResolver.resolveFolderPaths(path);
	result.insert(standard.begin(), standard.end());

	return result;
}

void Application::pushSearchPath(const std::string& path) {
	_standardPathResolver.pushSearchPath(path);
}

void Application::pushRelativeSearchPath(const std::string& path) {
	_standardPathResolver.pushRelativeSearchPath(path);
}

void Application::pushSearchPaths(const std::set<std::string>& paths) {
	_standardPathResolver.pushSearchPaths(paths);
}

void Application::popSearchPaths(size_t amount) {
	_standardPathResolver.popSearchPaths(amount);
}

void Application::setPathResolver(PathResolver::Pointer resolver) {
	_customPathResolver = resolver;
}

void Application::setShouldSilentPathResolverErrors(bool e) {
	_standardPathResolver.setSilentErrors(e);
}

const ApplicationIdentifier& Application::identifier() const {
	return _identifier;
}

/*
 * Service
 */
namespace {
static std::map<threading::ThreadIdentifier, RunLoop*> allRunLoops;
bool removeRunLoopFromMap(RunLoop* ptr) {
	bool found = false;
	auto i = allRunLoops.begin();
	while (i != allRunLoops.end())
	{
		if (i->second == ptr)
		{
			found = true;
			i = allRunLoops.erase(i);
		}
		else
		{
			++i;
		}
	}
	return found;
}
}

Application& application() {
	return Application::instance();
}

RunLoop& mainRunLoop() {
	return application().mainRunLoop();
}

RunLoop& backgroundRunLoop() {
	return application().backgroundRunLoop();
}

RunLoop& currentRunLoop() {
	auto i = allRunLoops.find(threading::currentThread());
	return (i == allRunLoops.end()) ? mainRunLoop() : *(i->second);
}

TimerPool::Pointer& mainTimerPool() {
	return application().mainRunLoop().mainTimerPool();
}

TimerPool::Pointer currentTimerPool() {
	return currentRunLoop().mainTimerPool();
}

void registerRunLoop(RunLoop& runLoop) {
	auto currentThread = threading::currentThread();
	ET_ASSERT(allRunLoops.count(currentThread) == 0);

	removeRunLoopFromMap(&runLoop);
	allRunLoops.insert({ currentThread, &runLoop });
}

void unregisterRunLoop(RunLoop& runLoop) {
	auto success = removeRunLoopFromMap(&runLoop);
	if (!success)
	{
		log::error("Attempt to unregister non-registered RunLoop");
	}
}

const std::string kSystemEventType = "kSystemEventType";
const std::string kSystemEventRemoteNotification = "kSystemEventRemoteNotification";
const std::string kSystemEventRemoteNotificationStatusChanged = "kSystemEventRemoteNotificationStatusChanged";
const std::string kSystemEventOpenURL = "kSystemEventOpenURL";

}
