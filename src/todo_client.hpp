// src/todo_client.hpp
#pragma once
#include <cpprest/http_client.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
struct Project {
    int id;
    std::string name;
    bool completed;
};
class TodoClient {
public:
    std::string serverUrl;
    // 获取所有项目列表
    std::vector<Project> getProjects() {
        web::http::client::http_client client(serverUrl);
        web::http::http_request req(web::http::methods::GET);
        req.set_request_uri(U("/projects"));
        req.headers().add(U("Accept"), U("application/json"));  // ← 加这行
        auto response = client.request(req).get();
        auto body = response.extract_json().get();
        std::vector<Project> projects;
        for (auto& item : body.as_array()) {
            Project p;
            p.id = item.at(U("id")).as_integer();
            p.name = item.at(U("name")).as_string();
            p.completed = item.at(U("completed")).as_bool();
            projects.push_back(p);
        }
        return projects;
    }
    // 根据 ID 获取单个项目
    Project getProject(int projectId) {
        web::http::client::http_client client(serverUrl);
        auto path = utility::string_t(U("/projects/")) +
                    utility::conversions::to_string_t(std::to_string(projectId));
        auto response = client.request(web::http::methods::GET, path).get();
        if (response.status_code() == 404) {
            throw std::runtime_error("Project not found");
        }
        auto body = response.extract_json().get();
        Project p;
        p.id = body.at(U("id")).as_integer();
        p.name = body.at(U("name")).as_string();
        p.completed = body.at(U("completed")).as_bool();
        return p;
    }
    // 创建新项目
    bool createProject(const std::string& name) {
        web::http::client::http_client client(serverUrl);
        nlohmann::json payload = {{"name", name}, {"completed", false}};
        web::http::http_request req(web::http::methods::POST);
        req.set_request_uri(U("/projects"));
        req.set_body(payload.dump(), web::http::details::mime_types::application_json);
        auto response = client.request(req).get();
        return response.status_code() == 201;
    }
};