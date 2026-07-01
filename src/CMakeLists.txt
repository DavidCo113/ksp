include(FetchContent)

FetchContent_Declare(
	cglm
	GIT_REPOSITORY	https://github.com/recp/cglm
	GIT_TAG			v0.8.5
)

set(CGLM_STATIC ON CACHE INTERNAL "")
set(CGLM_USE_C99 ON CACHE INTERNAL "")
set(MIRRORS https://butter.penguins.win/mirror)
FetchContent_MakeAvailable(cglm)

FetchContent_Declare(
	vxl
	GIT_REPOSITORY	https://github.com/xtreme8000/libvxl
	GIT_TAG			origin/master
)

FetchContent_MakeAvailable(vxl)

# SDL2: prefer a system install (brew, MacPorts, etc.) to avoid a full source
# build. Fall back to FetchContent only when nothing is found.
if(ENABLE_SDL)
	if(APPLE)
		# Homebrew — probe silently, no error if brew isn't present
		execute_process(
			COMMAND brew --prefix sdl2
			OUTPUT_VARIABLE SDL2_BREW_PREFIX
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET
			RESULT_VARIABLE SDL2_BREW_RESULT
		)
		if(SDL2_BREW_RESULT EQUAL 0 AND SDL2_BREW_PREFIX)
			list(APPEND CMAKE_PREFIX_PATH "${SDL2_BREW_PREFIX}")
		endif()
		# MacPorts
		if(EXISTS "/opt/local/lib/pkgconfig/sdl2.pc")
			list(APPEND CMAKE_PREFIX_PATH "/opt/local")
		endif()
	endif()

	find_package(SDL2 QUIET)

	if(SDL2_FOUND)
		message(STATUS "SDL2: using system install")
	else()
		message(STATUS "SDL2: not found locally, building from source via FetchContent")
		FetchContent_Declare(
			SDL2
			GIT_REPOSITORY	https://github.com/libsdl-org/SDL
			GIT_TAG			release-2.30.10
		)
		set(SDL_SHARED OFF CACHE BOOL "" FORCE)
		set(SDL_STATIC ON  CACHE BOOL "" FORCE)
		set(SDL_TEST   OFF CACHE BOOL "" FORCE)
		# Android: disable HIDAPI — its hid.cpp requires C++ runtime which a
		# pure-C executable doesn't link. Android's native joystick API is used
		# as fallback and works fine for a touch/gamepad game.
		if(ANDROID)
			set(SDL_HIDAPI          OFF CACHE BOOL "" FORCE)
			set(SDL_HIDAPI_JOYSTICK OFF CACHE BOOL "" FORCE)
		endif()
		FetchContent_MakeAvailable(SDL2)
		set(SDL2_USE_FETCHCONTENT TRUE)
	endif()
endif()

macro(download_file_if_it_doesnt_exist filename url)
	set(argn ${ARGN})
	list(LENGTH argn no_fallback)
	if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
		message(STATUS "Downloading ${filename}")
		file(DOWNLOAD ${url} ${CMAKE_CURRENT_SOURCE_DIR}/${filename} STATUS stat)
		list(GET stat 0 success)
		list(GET stat 1 error)
		if (NOT ${success})
			message(STATUS "Downloaded ${filename} successfully")
		else()
			file(REMOVE ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
			message(WARNING "Failed to download ${filename} (${error})")

			if(${no_fallback} EQUAL 0)
				foreach(mirror ${MIRRORS})
					string(SHA256 mirror_url_hash ${url})
					set(mirror_url ${mirror}/${mirror_url_hash})
					message(STATUS "Trying alternate URL ${mirror_url}")
					download_file_if_it_doesnt_exist(${filename} ${mirror_url} 1)
					if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
						# Success
						break()
					endif()
				endforeach()

				if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
					# Everything failed, exit
					message(FATAL_ERROR "Failed to download ${filename} from all sources.")
				endif()
			endif()
		endif()
	endif()
endmacro(download_file_if_it_doesnt_exist)

download_file_if_it_doesnt_exist(../bsresources.zip http://aos.party/bsresources.zip)

list(APPEND CLIENT_SOURCES aabb.c)
list(APPEND CLIENT_SOURCES camera.c)
list(APPEND CLIENT_SOURCES cameracontroller.c)
list(APPEND CLIENT_SOURCES chunk.c)
list(APPEND CLIENT_SOURCES config.c)
list(APPEND CLIENT_SOURCES dr_wav.c)
list(APPEND CLIENT_SOURCES file.c)
list(APPEND CLIENT_SOURCES font.c)
list(APPEND CLIENT_SOURCES glx.c)
list(APPEND CLIENT_SOURCES grenade.c)
list(APPEND CLIENT_SOURCES http.c)
list(APPEND CLIENT_SOURCES hud.c)
list(APPEND CLIENT_SOURCES chatlog.c chathistory.c)
list(APPEND CLIENT_SOURCES ini.c)
list(APPEND CLIENT_SOURCES list.c)
list(APPEND CLIENT_SOURCES main.c)
list(APPEND CLIENT_SOURCES map.c)
list(APPEND CLIENT_SOURCES matrix.c)
list(APPEND CLIENT_SOURCES model.c)
list(APPEND CLIENT_SOURCES skins.c)
list(APPEND CLIENT_SOURCES network.c)
list(APPEND CLIENT_SOURCES parson.c)
list(APPEND CLIENT_SOURCES particle.c)
list(APPEND CLIENT_SOURCES player.c)
list(APPEND CLIENT_SOURCES sound.c)
list(APPEND CLIENT_SOURCES stb_truetype.c)
list(APPEND CLIENT_SOURCES texture.c)
list(APPEND CLIENT_SOURCES tracer.c)
list(APPEND CLIENT_SOURCES weapon.c)
list(APPEND CLIENT_SOURCES window.c)
list(APPEND CLIENT_SOURCES utils.c)
list(APPEND CLIENT_SOURCES ping.c)
list(APPEND CLIENT_SOURCES log.c)
list(APPEND CLIENT_SOURCES minheap.c)
list(APPEND CLIENT_SOURCES hashtable.c)
list(APPEND CLIENT_SOURCES rpc.c)
list(APPEND CLIENT_SOURCES recorder.c)
list(APPEND CLIENT_SOURCES tesselator.c)
list(APPEND CLIENT_SOURCES microui.c)
list(APPEND CLIENT_SOURCES channel.c)
list(APPEND CLIENT_SOURCES entitysystem.c)
list(APPEND CLIENT_SOURCES demo.c)
# icon.rc is a Windows resource file — skip it on non-Windows platforms
if(WIN32)
	list(APPEND CLIENT_SOURCES ${KyroSpades_SOURCE_DIR}/resources/icon.rc)
endif()

# ── Target: shared library on Android, executable everywhere else ─────────────
# SDLActivity.java calls System.loadLibrary("main") which expects libmain.so.
# On all other platforms we keep the original executable target named "client".
if(ANDROID)
	add_library(main SHARED ${CLIENT_SOURCES})
	set(KS_TARGET main)
else()
	add_executable(client ${CLIENT_SOURCES})
	set(KS_TARGET client)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
	if(NOT ANDROID)
		set_property(TARGET ${KS_TARGET} PROPERTY WIN32_EXECUTABLE true)
	endif()
	target_compile_definitions(${KS_TARGET} PRIVATE NDEBUG)
endif()

set(GIT_COMMIT_HASH "?")

execute_process(
  COMMAND git describe --always --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

include(CheckIPOSupported)
check_ipo_supported(RESULT lto_supported OUTPUT error)

if(lto_supported AND NOT ANDROID)
	message(STATUS "IPO / LTO enabled")
	# set_property(TARGET ${KS_TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	message(STATUS "IPO / LTO not supported: <${error}>")
endif()

target_compile_definitions(${KS_TARGET} PRIVATE GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
target_compile_definitions(${KS_TARGET} PRIVATE DR_WAV_IMPLEMENTATION)
target_compile_definitions(${KS_TARGET} PRIVATE LOG_USE_COLOR)

if(ENABLE_TOUCH)
	target_compile_definitions(${KS_TARGET} PRIVATE USE_TOUCH)
endif()
if(ENABLE_ANDROID_FILE)
	target_compile_definitions(${KS_TARGET} PRIVATE USE_ANDROID_FILE)
endif()

target_compile_definitions(${KS_TARGET} PRIVATE KYROSPADES_MAJOR=${KYROSPADES_MAJOR})
target_compile_definitions(${KS_TARGET} PRIVATE KYROSPADES_MINOR=${KYROSPADES_MINOR})
target_compile_definitions(${KS_TARGET} PRIVATE KYROSPADES_PATCH=${KYROSPADES_PATCH})
target_compile_definitions(${KS_TARGET} PRIVATE KYROSPADES_VERSION="v${KYROSPADES_MAJOR}.${KYROSPADES_MINOR}.${KYROSPADES_PATCH}")

if(NOT ANDROID)
	set_target_properties(
		${KS_TARGET} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${KyroSpades_SOURCE_DIR}/build/KyroSpades
	)
endif()

set_target_properties(${KS_TARGET} PROPERTIES C_STANDARD 99)

set(CMAKE_PREFIX_PATH  ${CMAKE_PREFIX_PATH} ${KyroSpades_SOURCE_DIR}/deps)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${KyroSpades_SOURCE_DIR}/cmake/Modules)
set(OpenGL_GL_PREFERENCE LEGACY)

if(ENABLE_GLFW)
	find_package(glfw3 REQUIRED)
	target_link_libraries(${KS_TARGET} glfw3::glfw3)
	target_compile_definitions(${KS_TARGET} PRIVATE USE_GLFW)
endif()

if(ENABLE_SDL)
	if(SDL2_USE_FETCHCONTENT)
		target_link_libraries(${KS_TARGET} SDL2::SDL2-static)
	elseif(TARGET SDL2::SDL2)
		target_link_libraries(${KS_TARGET} SDL2::SDL2)
	else()
		target_link_libraries(${KS_TARGET} ${SDL2_LIBRARIES})
		target_include_directories(${KS_TARGET} PRIVATE ${SDL2_INCLUDE_DIRS})
	endif()
	target_compile_definitions(${KS_TARGET} PRIVATE USE_SDL)
endif()

if(ENABLE_SOUND)
	find_package(OpenAL REQUIRED)
	target_link_libraries(${KS_TARGET} ${OPENAL_LIBRARY})
	if(ANDROID)
		# Static openal-soft is C++ and uses the OpenSL ES backend + logcat.
		# Our target is pure C, so none of these get pulled in implicitly.
		target_link_libraries(${KS_TARGET} OpenSLES log c++_static c++abi)
	endif()
	target_compile_definitions(${KS_TARGET} PRIVATE USE_SOUND)
endif()

find_package(enet REQUIRED)
find_package(deflate REQUIRED)
find_package(Threads REQUIRED)

# ── FFmpeg (prefer vendored Linux libs, fall back to system) ─────────────────
set(FFMPEG_DIR ${KyroSpades_SOURCE_DIR}/deps/ffmpeg)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${FFMPEG_DIR}/lib/libavcodec.a")
	message(STATUS "FFmpeg: using vendored libs from deps/ffmpeg/")
	target_include_directories(${KS_TARGET} PRIVATE ${FFMPEG_DIR}/include)
	target_link_directories(${KS_TARGET} PRIVATE ${FFMPEG_DIR}/lib)
	target_link_libraries(${KS_TARGET}
	        avcodec avformat avutil swresample swscale)
else()
	# macOS / Windows / Android — use system FFmpeg via pkg-config
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(AVCODEC  libavcodec)
		pkg_check_modules(AVFORMAT libavformat)
		pkg_check_modules(AVUTIL   libavutil)
		pkg_check_modules(SWRESAMPLE libswresample)
		pkg_check_modules(SWSCALE  libswscale)
	endif()
	if(AVCODEC_FOUND AND AVFORMAT_FOUND AND AVUTIL_FOUND AND SWRESAMPLE_FOUND AND SWSCALE_FOUND)
		target_include_directories(${KS_TARGET} PRIVATE
			${AVCODEC_INCLUDE_DIRS} ${AVFORMAT_INCLUDE_DIRS}
			${AVUTIL_INCLUDE_DIRS} ${SWRESAMPLE_INCLUDE_DIRS} ${SWSCALE_INCLUDE_DIRS})
		target_link_libraries(${KS_TARGET}
			${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES}
			${AVUTIL_LIBRARIES} ${SWRESAMPLE_LIBRARIES} ${SWSCALE_LIBRARIES})
	else()
		message(STATUS "FFmpeg: system packages not found — recorder/video features disabled")
		target_compile_definitions(${KS_TARGET} PRIVATE NO_FFMPEG)
	endif()
endif()

# PulseAudio for audio capture (Linux only — skip on Windows/macOS/Android)
find_package(PulseAudio QUIET)
if(PulseAudio_FOUND)
	target_link_libraries(${KS_TARGET} pulse-simple pulse)
	target_compile_definitions(${KS_TARGET} PRIVATE USE_PULSEAUDIO)
	message(STATUS "PulseAudio: found — audio capture enabled")
else()
	message(STATUS "PulseAudio: not found — audio capture disabled")
endif()

# ── OpenGL / OpenGL ES ────────────────────────────────────────────────────────
if(ANDROID)
	# On Android, EGL and GLESv2 are system libraries provided by the NDK.
	# CMake's built-in FindOpenGL doesn't understand this, so we define the
	# imported targets directly instead of calling find_package(OpenGL).
	if(NOT TARGET OpenGL::EGL)
		add_library(OpenGL::EGL INTERFACE IMPORTED GLOBAL)
		set_target_properties(OpenGL::EGL PROPERTIES INTERFACE_LINK_LIBRARIES "EGL")
	endif()
	if(NOT TARGET OpenGL::OpenGL)
		add_library(OpenGL::OpenGL INTERFACE IMPORTED GLOBAL)
		set_target_properties(OpenGL::OpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "GLESv2")
	endif()
	set(OPENGL_FOUND TRUE)
	set(OPENGL_LIBRARIES "GLESv2;GLESv1_CM")
	set(OPENGL_INCLUDE_DIR "")
	set(OPENGL_EGL_INCLUDE_DIRS "")
	target_link_libraries(${KS_TARGET} OpenGL::EGL)
	# libandroid provides the AAssetManager API used by file_dir_list() to
	# enumerate APK assets (random menu backgrounds). SDL links it for its own
	# use, but our libmain.so references the symbols directly and needs it too.
	target_link_libraries(${KS_TARGET} android)
	target_compile_definitions(${KS_TARGET} PRIVATE OPENGL_ES)
elseif(NOT ENABLE_OPENGLES)
	find_package(OpenGL REQUIRED)
	find_package(GLEW REQUIRED)
	target_link_libraries(${KS_TARGET} GLEW::GLEW)
else()
	find_package(OpenGL REQUIRED COMPONENTS EGL)
	target_link_libraries(${KS_TARGET} OpenGL::EGL)
	target_compile_definitions(${KS_TARGET} PRIVATE OPENGL_ES)
endif()

if(ENABLE_RPC)
	target_link_libraries(${KS_TARGET} discord-rpc stdc++)
	target_compile_definitions(${KS_TARGET} PRIVATE USE_RPC DISCORD_DISABLE_IO_THREAD)
endif()
if(DEFINED ENV{BUILD_NUMBER})
	target_compile_definitions(${KS_TARGET} PRIVATE JENKINS_BUILD="$ENV{BUILD_NUMBER}")
endif()

if(WIN32)
	target_compile_definitions(${KS_TARGET} PRIVATE LIBDEFLATE_STATIC)
endif()

target_link_libraries(${KS_TARGET} ${CMAKE_THREAD_LIBS_INIT} ${OPENGL_LIBRARIES} enet::enet deflate::deflate m cglm vxl)
target_include_directories(${KS_TARGET} PRIVATE ${OPENAL_INCLUDE_DIR} ${OPENAL_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR} ${OPENGL_EGL_INCLUDE_DIRS})

# ── Post-build resource copy (desktop only) ───────────────────────────────────
# On Android, resources are packaged as APK assets by Gradle — no post-build
# copy needed and the paths wouldn't exist in the Gradle build environment.
if(NOT ANDROID)
	add_custom_command(
		TARGET ${KS_TARGET}
		POST_BUILD
		COMMENT "Copying resources..."
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${KyroSpades_SOURCE_DIR}/resources ${KyroSpades_SOURCE_DIR}/build/KyroSpades
	)

	add_custom_command(
		TARGET ${KS_TARGET}
		POST_BUILD
		COMMENT "Extracting additional third-party assets..."
		DEPENDS ${KyroSpades_SOURCE_DIR}/bsresources.zip
		WORKING_DIRECTORY ${KyroSpades_SOURCE_DIR}/build/KyroSpades
		COMMAND ${CMAKE_COMMAND} -E tar xzf ${KyroSpades_SOURCE_DIR}/bsresources.zip
	)

	add_custom_command(
		TARGET ${KS_TARGET}
		POST_BUILD
		COMMENT "Copying overrides..."
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${KyroSpades_SOURCE_DIR}/resources.override ${KyroSpades_SOURCE_DIR}/build/KyroSpades
	)

	install(
		TARGETS ${KS_TARGET}
		RUNTIME
		DESTINATION .
	)

	install(
		FILES "${CMAKE_BINARY_DIR}/KyroSpades/config.ini" "${CMAKE_BINARY_DIR}/KyroSpades/OpenAL32.dll"
		DESTINATION .
		PERMISSIONS	OWNER_EXECUTE OWNER_WRITE OWNER_READ
					GROUP_EXECUTE GROUP_READ
	)

	install(
		DIRECTORY "${CMAKE_BINARY_DIR}/KyroSpades/fonts"
		DESTINATION .
		FILES_MATCHING PATTERN "*.ttf"
	)

	install(
		DIRECTORY "${CMAKE_BINARY_DIR}/KyroSpades/kv6"
		DESTINATION .
		FILES_MATCHING PATTERN "*.kv6"
	)

	install(
		DIRECTORY "${CMAKE_BINARY_DIR}/KyroSpades/png"
		DESTINATION .
		FILES_MATCHING PATTERN "*.png"
	)

	install(
		DIRECTORY "${CMAKE_BINARY_DIR}/KyroSpades/wav"
		DESTINATION .
		FILES_MATCHING PATTERN "*.wav"
	)

	install(DIRECTORY DESTINATION "cache")
	install(DIRECTORY DESTINATION "logs")
	install(DIRECTORY DESTINATION "screenshots")

	set(CPACK_GENERATOR NSIS)
	set(CPACK_PACKAGE_NAME "KyroSpades")
	set(CPACK_PACKAGE_VENDOR "github.com")
	set(CPACK_PACKAGE_VERSION "v${KYROSPADES_MAJOR}.${KYROSPADES_MINOR}.${KYROSPADES_PATCH}")
	set(CPACK_PACKAGE_VERSION_MAJOR "${KYROSPADES_MAJOR}")
	set(CPACK_PACKAGE_VERSION_MINOR "${KYROSPADES_MINOR}")
	set(CPACK_PACKAGE_VERSION_PATCH "${KYROSPADES_PATCH}")
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_NAME}")
	set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
	set(CPACK_STRIP_FILES TRUE)
	set(CPACK_NSIS_MENU_LINKS "client" "KyroSpades")
	set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
	set(CPACK_PACKAGE_CHECKSUM SHA256)

	INCLUDE(CPack)
endif()
