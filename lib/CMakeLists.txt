foreach(required_submodule IN ITEMS librdr3 pugixml)
	if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${required_submodule}/CMakeLists.txt")
		message(FATAL_ERROR "Missing submodule '${required_submodule}'. Run 'git submodule update --init --recursive' before configuring.")
	endif()
endforeach()

add_subdirectory(ImGuiFileDialog)
add_subdirectory(glm)
add_subdirectory(librdr3)
add_subdirectory(Recast)
add_subdirectory(pugixml)

include( imgui.cmake )