

#include "ChartographMessenger.grpc.pb.h"

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#include <AbstractApplication.h>
#include "Renderer/BatchRenderer.h"
#include <imgui_internal.h>

enum IndexMode {
	CONTINUOUS,
	DISCRETE
};

struct Vec3Compare {
	bool operator()(const glm::vec3& a, const glm::vec3& b) const {
		if (a.x != b.x)
			return a.x < b.x;
		if (a.y != b.y)
			return a.y < b.y;
		return a.z < b.z;
	}
};

struct Vec3Hash {
	std::size_t operator()(const glm::vec3& v) const {
		// Hash combining algorithm, for instance, boost's hash_combine
		// or use any other hash combining algorithm you prefer.
		std::size_t seed = 0;
		// Example of hash combining algorithm
		seed ^= std::hash<float>()(v.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<float>()(v.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		seed ^= std::hash<float>()(v.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		return seed;
	}
};

struct lines {
	std::vector<glm::vec3> points;
	std::vector<unsigned int> indices;

	void switchIndexMode() {
		if (indexMode == CONTINUOUS) {
			indexMode = DISCRETE;
			indices.clear();
			for (int i = 0; i < points.size(); i++) {
				indices.push_back(i);
			}
		}
		else if (indexMode == DISCRETE) {
			indexMode = CONTINUOUS;
			indices.clear();
			if (points.size() == 1) return;
			for (int i = 0; i < points.size() - 1; i++) {
				indices.push_back(((unsigned int)i));
				indices.push_back(((unsigned int)i + 1));
			}
		}
	}

	IndexMode getIndexMode() {
		return indexMode;
	}

	void addPoint(glm::vec3 point) {
		points.push_back(point);
		
		if (points.size() == 1 && indexMode == CONTINUOUS) {
			return;
		}
		addIndex();
	}
private:
	IndexMode indexMode = CONTINUOUS;

	void addIndex() {
		if (indexMode == CONTINUOUS) {
			indices.push_back(((unsigned int)points.size() - 2));
			indices.push_back(((unsigned int)points.size() - 1));
		}
		else if (indexMode == DISCRETE) {
			indices.push_back(((unsigned int)points.size() - 1));
		}
	}
};



struct triangles {
private:
	std::unordered_set<glm::vec3, Vec3Hash> points;
public:
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<glm::vec4> colors;

	void addTriangle(glm::vec3 point1, glm::vec3 point2, glm::vec3 point3, glm::vec4 color) {

		//auto itrp1 = points.insert(point1);
		//if (itrp1.second) {
			vertices.push_back(point1);
			indices.push_back((vertices.size() - 1));
			colors.push_back(color);
		//}
		//else {
		//	indices.push_back(std::distance(vertices.begin(),std::find(vertices.begin(), vertices.end(), point1)));
		//	
		//}

		//auto itrp2 = points.insert(point2);
		//if (itrp2.second) {
			vertices.push_back(point2);
			indices.push_back((vertices.size() - 1));
			colors.push_back(color);
		//}
		//else {
		//	indices.push_back((std::distance(vertices.begin(), std::find(vertices.begin(), vertices.end(), point2))));
		//}

		//auto itrp3 = points.insert(point3);
		//if (itrp3.second) {
			vertices.push_back(point3);
			indices.push_back((vertices.size() - 1));
			colors.push_back(color);
		//}
		//else {
		//	indices.push_back((std::distance(vertices.begin(), std::find(vertices.begin(), vertices.end(), point3))));
		//}

	}

	void addTriangle(glm::vec3 point1, glm::vec3 point2, glm::vec3 point3, std::array<glm::vec4, 3> perVertexColor) {
		vertices.push_back(point1);
		indices.push_back((vertices.size() - 1));
		colors.push_back(perVertexColor[0]);


		vertices.push_back(point2);
		indices.push_back((vertices.size() - 1));
		colors.push_back(perVertexColor[1]);


		vertices.push_back(point3);
		indices.push_back((vertices.size() - 1));
		colors.push_back(perVertexColor[2]);
	}

};

static std::map<std::string, lines> line_plots;
static std::map<std::string, triangles> triangle_plots;

class ChartoGraphMessengerImpl : public ChartographMessenger::GraphPlotter::Service {
	grpc::Status CreatePlot(grpc::ServerContext* context, const ChartographMessenger::GraphData* request, ChartographMessenger::PlotResponse* response) override {
		std::string message("Plot Created");

		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		if (request->graph_type() == ChartographMessenger::GraphType::LINE) {
			line_plots.insert({ request->plotid(), lines() });
		}
		else if (request->graph_type() == ChartographMessenger::GraphType::TRIANGLE) {
			triangle_plots.insert({ request->plotid(), triangles() });
		}
		else {
			return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Graph Type");
		}

		response->set_plotid(request->plotid());
		response->set_success(true);
		response->set_message(message);
		return grpc::Status::OK;
	}

	grpc::Status UpdatePlot(grpc::ServerContext* context, const ChartographMessenger::UpdateData* request, ChartographMessenger::PlotResponse* response) override {

		// Lock to ensure thread safety
		std::lock_guard<std::mutex> lock(mutex_);

		if (request->plotid().empty()) {
			return plotIdError(response);
		}

		if (request->has_new_data_points()) {
			for (ChartographMessenger::Point point : request->new_data_points().points()) {
				line_plots[request->plotid()].addPoint(glm::vec3(point.x(), point.y(), 0.0f));
			}
		}
		
		else if (request->has_new_triangle()) {
			triangle_plots[request->plotid()].addTriangle(
				glm::vec3(request->new_triangle().p1().x(), request->new_triangle().p1().y(), request->new_triangle().p1().z()),
				glm::vec3(request->new_triangle().p2().x(), request->new_triangle().p2().y(), request->new_triangle().p2().z()),
				glm::vec3(request->new_triangle().p3().x(), request->new_triangle().p3().y(), request->new_triangle().p3().z()),
				glm::vec4(request->new_triangle().color().r(), request->new_triangle().color().g(), request->new_triangle().color().b(), request->new_triangle().color().a()));
		}

		else if (request->has_new_per_vertex_color_triangle()) {
						triangle_plots[request->plotid()].addTriangle(
							{ glm::vec3(request->new_per_vertex_color_triangle().p1().x(), request->new_per_vertex_color_triangle().p1().y(), request->new_per_vertex_color_triangle().p1().z()) },
							{ glm::vec3(request->new_per_vertex_color_triangle().p2().x(), request->new_per_vertex_color_triangle().p2().y(), request->new_per_vertex_color_triangle().p2().z()) },
							{ glm::vec3(request->new_per_vertex_color_triangle().p3().x(), request->new_per_vertex_color_triangle().p3().y(), request->new_per_vertex_color_triangle().p3().z()) },
							{
								glm::vec4(request->new_per_vertex_color_triangle().c1().r(), request->new_per_vertex_color_triangle().c1().g(), request->new_per_vertex_color_triangle().c1().b(), request->new_per_vertex_color_triangle().c1().a()),
								glm::vec4(request->new_per_vertex_color_triangle().c2().r(), request->new_per_vertex_color_triangle().c2().g(), request->new_per_vertex_color_triangle().c2().b(), request->new_per_vertex_color_triangle().c2().a()),
								glm::vec4(request->new_per_vertex_color_triangle().c3().r(), request->new_per_vertex_color_triangle().c3().g(), request->new_per_vertex_color_triangle().c3().b(), request->new_per_vertex_color_triangle().c3().a())
							}
						);
		}

		else if (request->has_new_triangles()) {
			for (ChartographMessenger::Triangle triangle : request->new_triangles().triangles()) {
				triangle_plots[request->plotid()].addTriangle(
					glm::vec3(triangle.p1().x(), triangle.p1().y(), triangle.p1().z()), 
					glm::vec3(triangle.p2().x(), triangle.p2().y(), triangle.p2().z()), 
					glm::vec3(triangle.p3().x(), triangle.p3().y(), triangle.p3().z()),
					glm::vec4(triangle.color().r(), triangle.color().g(), triangle.color().b(), triangle.color().a()));
			}
		}

		std::string message("Point added");
		response->set_plotid(request->plotid());
		response->set_success(true);
		response->set_message(message);
		return grpc::Status::OK;
	}
private:

	// Mutex for thread safety
	std::mutex mutex_;

	inline grpc::Status plotIdError(ChartographMessenger::PlotResponse* response) {
		return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Plot Id");
	}

	inline grpc::Status graphTypeError(ChartographMessenger::PlotResponse* response) {
		return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Graph Type");
	}
};

static std::shared_ptr<grpc::Channel> serverChannel;

void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
{
	ImGuiIO& io = ImGui::GetIO();
	auto boldFont = io.Fonts->Fonts[0];

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	ImGui::Text("%s", label.c_str());
	ImGui::NextColumn();

	ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
	ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("X", buttonSize))
		values.x = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Y", buttonSize))
		values.y = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();
	ImGui::SameLine();

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushFont(boldFont);
	if (ImGui::Button("Z", buttonSize))
		values.z = resetValue;
	ImGui::PopFont();
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::PopItemWidth();

	ImGui::PopStyleVar();

	ImGui::Columns(1);

	ImGui::PopID();
}

class ObjectLayer : public GUI::Layer {
	glm::vec3 currentPoint = {0,0,0};
	bool showPoint = false;
	float size = 0.1f;
	glm::vec4 color = { 1.0, 0.0, 0.0, 1.0 };
public:
	ObjectLayer() : Layer("ObjectLayer") {
	}
	~ObjectLayer() {}

	void OnAttach() {}

	void OnDrawUpdate() {
		if (showPoint)
			Graphics::BatchRenderer::DrawCircle(currentPoint, size, color);

		for (const auto& plot : line_plots) {
			Graphics::BatchRenderer::DrawLines(plot.second.points, plot.second.indices,glm::vec4(1.0));
		}

		for (const auto& plot : triangle_plots) {
			Graphics::BatchRenderer::DrawMesh(plot.second.vertices, plot.second.indices, plot.second.colors);
		}
	}

	void OnImGuiRender() {
		ImGui::Begin("Debug point");
		ImGui::Checkbox("Show Point", &showPoint);
		if (showPoint) {
			DrawVec3Control("Position", currentPoint);
			ImGui::DragFloat("Size", &size, 1.0f, 0.0f, FLT_MAX, "%.3f");
			ImGui::ColorEdit4("MyColor##3", (float*)&color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
			this->UpdateLayer(true);
		}
		ImGui::End();

		ImGui::Begin("Line Plots");
		for (auto& plot : line_plots) {
			if (ImGui::TreeNode(plot.first.c_str())) {
				if (ImGui::Button("Switch Index mode")) {
					plot.second.switchIndexMode();
				}
				ImGui::SameLine();
				ImGui::Text("%s", plot.second.getIndexMode() == CONTINUOUS ? "Continuous" : "Discrete");
				if (ImGui::Button("Remove")) {
					line_plots.erase(plot.first);
					ImGui::TreePop();
					break;
				}
				for (const auto& point : plot.second.points) {
					ImGui::Text("X : %f , Y : %f , Z : %f", point.x, point.y, point.z);
					ImGui::SameLine();
					ImGui::Button("Show");
				}
				ImGui::TreePop();

			}
		}
		ImGui::End();

		ImGui::Begin("Triangle Plots");
		for (auto& plot : triangle_plots) {
			if (ImGui::TreeNode(plot.first.c_str())) {
				if (ImGui::Button("Remove")) {
					triangle_plots.erase(plot.first);
					ImGui::TreePop();
					break;
				}
				int counter = 0;
				for (auto point : plot.second.vertices) {
					ImGui::Text("X : %f , Y : %f , Z : %f", point.x, point.y, point.z);
					ImGui::SameLine();
					std::string showButton = "Show " + std::to_string(counter);
					if (ImGui::Button(showButton.c_str())) {
						currentPoint = point;
					}
					counter++;
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}
};

class ChartoGrapher : public GUI::AbstractApplication {
private:
public:
	ChartoGrapher(const GUI::ApplicationSpecification& spec)
		:AbstractApplication(spec)
	{
		PushLayer(new ObjectLayer());
	}

	~ChartoGrapher() {
		std::cout << "ChartoGrapher Destructor" << std::endl;
	}
};


ChartoGrapher* CreateApplication(GUI::ApplicationCommandLineArgs args)
{
	GUI::ApplicationSpecification spec;
	spec.Name = "ChartoGrapher";
	spec.CommandLineArgs = args;
	return new ChartoGrapher(spec);
}

void GrpcThread() {
	//Note: Reflection in postman will not work with this line
	//grpc::EnableDefaultHealthCheckService(true);
	grpc::reflection::InitProtoReflectionServerBuilderPlugin();

	std::string server_address("0.0.0.0:50051");
	ChartoGraphMessengerImpl service;

	grpc::ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
	grpc::ChannelArguments args;
	serverChannel = server->InProcessChannel(args);

	LOG_INFO_STREAM << "Server listening on " << server_address;
	server->Wait();
}


int main(int argc, char** argv)
{
	auto app = CreateApplication({ argc, argv });

	std::thread grpcThread(GrpcThread);

	grpcThread.detach();
	
	app->Run();

	delete app;
}
