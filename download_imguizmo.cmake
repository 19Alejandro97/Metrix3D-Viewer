set(URL_H "https://raw.githubusercontent.com/CedricGuillemet/ImGuizmo/master/ImGuizmo.h")
set(URL_CPP "https://raw.githubusercontent.com/CedricGuillemet/ImGuizmo/master/ImGuizmo.cpp")

file(DOWNLOAD ${URL_H} "src/vendor/ImGuizmo/ImGuizmo.h" STATUS status_h LOG log_h)
file(DOWNLOAD ${URL_CPP} "src/vendor/ImGuizmo/ImGuizmo.cpp" STATUS status_cpp LOG log_cpp)

message(STATUS "Downloaded H: ${status_h}")
message(STATUS "Downloaded CPP: ${status_cpp}")
